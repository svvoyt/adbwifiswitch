
#include <cassert>

#include "FilePoller.h"
#include "Logger.h"

FilePoller::FilePoller(bool single_threaded)
    : m_singleThreaded(single_threaded)
{
}

FilePoller::HandlerId FilePoller::addHandler(std::shared_ptr<FileHandler> handler)
{
    assert(handler);
    auto lck( getLock() );

    auto seq = getNextSeq();
    if (seq != BadHandlerId) {
        m_hndList.emplace( seq, handler );
    }
    return seq;
}

void FilePoller::exec()
{
    std::vector<pollfd> pfd;
    std::vector<FilePoller::HandlerId> ind;
    while( pollHandlers(std::chrono::milliseconds::max(), pfd, ind) );
}

bool FilePoller::pollHandlers(std::chrono::milliseconds timeout)
{
    std::vector<pollfd> pfd;
    std::vector<FilePoller::HandlerId> ind;
    return pollHandlers(timeout, pfd, ind);
}

void FilePoller::removeHandler(FilePoller::HandlerId hndId)
{
    auto lck( getLock() );
    m_hndList.erase( hndId );
}


inline std::unique_lock<std::mutex> FilePoller::getLock()
{
    return m_singleThreaded ? std::unique_lock<std::mutex>() :
                              std::unique_lock<std::mutex>(m_lock);
}

FilePoller::HandlerId FilePoller::getNextSeq()
{
    auto startSeq = m_seq++;
    while((m_seq == BadHandlerId || m_hndList.find(m_seq) != m_hndList.cend()) &&
          m_seq != startSeq) m_seq++;
    return m_seq;
}

bool FilePoller::pollHandlers(std::chrono::milliseconds timeout,
                      std::vector<pollfd> &pfd, std::vector<FilePoller::HandlerId> &ind)
{
    std::size_t pfd_size = pfd.size();
    std::size_t ind_size = ind.size();
    if (ind_size > pfd_size) {
        ind.resize( pfd_size );
        ind_size = pfd_size;
    } else if (ind_size < pfd_size) {
        pfd.resize( ind_size );
        pfd_size = ind_size;
    }
    std::size_t fd_count = 0;

    HandlerId firstTimer = BadHandlerId;
    bool moreTimers = false;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    {
        auto lck( getLock() );

        for(auto it = m_hndList.begin(); it != m_hndList.end();) {
            auto handler = it->second.lock();
            if (handler) {
                if (!handler->isEnabled())
                    continue;

                struct pollfd spfd;
                spfd.fd = handler->getFd();
                spfd.events = POLLIN | POLLERR | (handler->writeRequired() ? POLLOUT : 0);
                spfd.revents = 0;
                const HandlerId hId = it->first;
                if (fd_count < pfd_size) {
                    pfd[fd_count] = spfd;
                    ind[fd_count] = hId;
                } else {
                    pfd.push_back( spfd );
                    ind.push_back( hId );
                }
                fd_count++;

                // timers
                const auto timer = handler->getClosestTime();
                const bool empty_timer = timer == timer.max();
                bool use_timer = false;
                if (!empty_timer) {
                    if (timer <= now) {
                        use_timer = true;
                        timeout = timeout.zero();
                    } else if (timeout != timeout.zero()) {
                            std::chrono::steady_clock::duration diff = now - timer;
                            if (diff < timeout) {
                                timeout = std::chrono::duration_cast<decltype(timeout)>( diff );
                                use_timer = true;
                            }
                    }
                    if (use_timer) {
                        moreTimers = firstTimer != BadHandlerId;
                        firstTimer = hId;
                    }
                }
                ++it;
            } else {
                it  = m_hndList.erase( it );
            }
        }
    }

    if (pfd.empty()) return true;

    int poll_timeo = timeout == timeout.max() ? -1 : static_cast<int>( timeout.count() );

    LOGD(true, "Running poll. fd count %zu timeout %d", fd_count, poll_timeo);

    const int pret = poll( pfd.data(), static_cast<nfds_t>(fd_count), poll_timeo );

    if (pret < 0) {
        LOGE(true, "Poll error %d", errno);
        return false;
    }

    const bool has_fileevents = pret > 0;

    if (has_fileevents) {
        for(std::size_t i = 0; i < fd_count; i++) {
            const struct pollfd &spfd = pfd[i];
            if (spfd.revents != 0) {
                LOGD((spfd.revents & ~(POLLERR|POLLIN|POLLOUT)), "revent = 0x%x", spfd.revents);
                auto lck( getLock() );
                auto it = m_hndList.find( ind[i] );
                if (it != m_hndList.cend()) {
                    auto handler = it->second.lock();
                    if (handler) {
                        bool henabled = handler->isEnabled();
                        if (henabled && spfd.revents & POLLIN) {
                            handler->onReadyToRead();
                            henabled = handler->isEnabled();
                        }
                        if (henabled && spfd.revents & POLLERR) {
                            handler->onError();
                            henabled = handler->isEnabled();
                        }
                        if (henabled && spfd.revents & POLLOUT) {
                            handler->onReadyToWrite();
                        }
                    } else {
                        LOGD(true, "Handler for fd %d was destroyed", spfd.fd);
                    }
                } else {
                    LOGD(true, "Handler for fd %d was removed", spfd.fd);
                }
            }
        }
    }

    if (firstTimer != BadHandlerId) {
        now = std::chrono::steady_clock::now();
        if (moreTimers) {
            HandlerId curr = 0;
            while(1) {
                auto lck( getLock() );
                auto it = m_hndList.upper_bound( curr );
                decltype(it->second.lock()) handler;
                for(; it != m_hndList.cend(); ++it) {
                    handler = it->second.lock();
                    if (handler && handler->isEnabled())
                        break;
                }
                if (it != m_hndList.cend()) {
                    curr = it->first;
                    handler->checkTimer( now );
                } else {
                    break;
                }
            }
        } else {
            auto lck( getLock() );
            auto it = m_hndList.find( firstTimer );
            if (it != m_hndList.cend()) {
                auto handler = it->second.lock();
                if (handler && handler->isEnabled()) {
                    handler->checkTimer( now );
                }
            }
        }
    }

    return true;
}
