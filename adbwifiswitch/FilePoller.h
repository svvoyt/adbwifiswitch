#ifndef FILEPOLLER_H
#define FILEPOLLER_H

#include <poll.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "FileHandler.h"

class FilePoller {
public:
    typedef unsigned int HandlerId;
    static const HandlerId BadHandlerId = 0;

    FilePoller(bool single_threaded = true);

    HandlerId addHandler( std::shared_ptr<FileHandler> handler);
    bool pollHandlers(std::chrono::milliseconds timeout);
    void removeHandler( HandlerId hndId );

private:
    typedef std::map<HandlerId, std::weak_ptr<FileHandler> > HandlerList;

    std::unique_lock<std::mutex> getLock();
    HandlerId getNextSeq();
    bool pollHandlers(std::chrono::milliseconds, std::vector<pollfd> &pfd, std::vector<HandlerId> &ind);

    HandlerList m_hndList;
    std::mutex m_lock;
    bool m_singleThreaded;
    HandlerId m_seq = BadHandlerId;
};

#endif // FILEPOLLER_H
