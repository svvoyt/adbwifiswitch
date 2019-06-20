#include <unistd.h>

#include "FileHandler.h"


namespace {
enum Flags {
    fState = 0x1,
    fPollOut = 0x2,
};
}

FileHandler::FileHandler(int fd)
    : m_fd(fd), m_flags(0)
{

}

FileHandler::~FileHandler()
{
    if (m_fd != -1) {
        close( m_fd );
    }
}

void FileHandler::checkTimer(std::chrono::steady_clock::time_point now)
{
    while(1) {
        auto it = m_timers.cbegin();
        if (it == m_timers.cend() || it->first > now)
            break;
        onTimer( it->second );
    }
}

std::chrono::steady_clock::time_point FileHandler::getClosestTime() const
{
    auto it = m_timers.cbegin();
    if (it != m_timers.cend()) return it->first;
    return std::chrono::steady_clock::time_point::max();
}

bool FileHandler::isEnabled() const
{
    return isFlag(fState);
}

bool FileHandler::writeRequired() const
{
    return isFlag(fPollOut);
}

void FileHandler::setState(bool enable)
{
    setFlag( fState, enable );
}

bool FileHandler::setTimer(unsigned int timerId, std::chrono::milliseconds ms)
{
    auto now = std::chrono::steady_clock::now() + ms;

    m_timers.emplace( now, timerId );
    return true;
}

void FileHandler::setWriteRequest(bool enable)
{
    setFlag( Flags::fPollOut, enable);
}


// FileHandler:: private methods

inline bool FileHandler::isFlag(int flag) const
{
    return (m_flags & flag) == flag;
}

inline void FileHandler::setFlag(int flag, bool enable)
{
    if (enable) m_flags |= flag;
    else m_flags &= ~flag;
}
