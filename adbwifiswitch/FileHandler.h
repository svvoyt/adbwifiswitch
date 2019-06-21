#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <chrono>
#include <map>

class FileHandler {
public:
    FileHandler(int fd);
    virtual ~FileHandler();

    int getFd() const {return m_fd;}

    void checkTimer(std::chrono::steady_clock::time_point now);
    std::chrono::steady_clock::time_point getClosestTime() const;
    bool isEnabled() const;
    bool writeRequired() const;
    void setState( bool enable );
    bool startTimer(unsigned int timerId, std::chrono::milliseconds ms, bool reset_prev = true);
    bool stopTimer(unsigned int timerId);
    void setWriteRequest(bool enable);

    virtual bool onTimer( unsigned int timerId ) {return false;}
    virtual bool onReadyToRead() = 0;
    virtual bool onReadyToWrite() = 0;
    virtual bool onError() = 0;

private:

    bool isFlag(int flag) const;
    void setFlag( int flag, bool enable );

    typedef std::multimap<std::chrono::steady_clock::time_point, unsigned int> TimerList;

    int m_fd;
    int m_flags;
    TimerList m_timers;
};

#endif // FILEHANDLER_H
