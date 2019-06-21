#ifndef ADBTASK_H
#define ADBTASK_H

#include <functional>
#include <memory>
#include <string>

#include "AdbContext.h"


class AdbTask {
public:
    enum Res {
        Next, Continue, Fail
    };
    
    AdbTask( std::shared_ptr<AdbContext> ctx );
    virtual ~AdbTask();
    
    virtual void cleanup() {}
    virtual bool start() = 0;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size);
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId);

protected:
    
    bool isStdin(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdIn;}
    bool isStdout(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdOut;}
    bool isStderr(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdErr;}
    
    std::shared_ptr<AdbContext> m_context;
};

class AdbTaskWaitFirstPrompt : public AdbTask {
public:
    using AdbTask::AdbTask;
    virtual void cleanup() override;
    virtual bool start() override;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size) override;
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId) override;
};

#endif // ADBTASK_H
