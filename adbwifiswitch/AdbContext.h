#ifndef ADBCONTEXT_H
#define ADBCONTEXT_H

#include <chrono>
#include <memory>


class Config;

class AdbContext {
public:
    enum FStream {
        fsStdIn, fsStdOut, fsStdErr
    };
    
    AdbContext(std::shared_ptr<Config> cfg);
    virtual ~AdbContext();

    Config *config() {return m_config.get();}
    const Config *config() const {return m_config.get();}
    std::shared_ptr<Config> configShared() {return m_config;}
    
    virtual bool writeStdIn(const void *buf, std::size_t size) = 0;
    virtual bool timerCtl(FStream fstream, unsigned int timerId, bool start,
                          std::chrono::milliseconds ms=std::chrono::milliseconds::zero()) = 0;
    
private:
    std::shared_ptr<Config> m_config;
};

#endif // ADBCONTEXT_H
