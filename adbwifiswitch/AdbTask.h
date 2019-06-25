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
    
    enum State {
        Idle, Running, Stopped
    };
    
    AdbTask( std::shared_ptr<AdbContext> ctx );
    virtual ~AdbTask();
    
    virtual void cleanup() {}
    virtual bool start() = 0;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size);
    virtual Res onError(AdbContext::FStream fstream);
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId);

protected:
    
    bool isRunning() const {return m_state == State::Running;}
    bool isStdin(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdIn;}
    bool isStdout(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdOut;}
    bool isStderr(AdbContext::FStream fstream) const {return fstream == AdbContext::FStream::fsStdErr;}
    
    void setState(State state) {m_state = state;}
    
    std::shared_ptr<AdbContext> m_context;
    State m_state = State::Idle;
};

class AdbTaskWaitFirstPrompt : public AdbTask {
public:
    using AdbTask::AdbTask;
    virtual void cleanup() override;
    virtual bool start() override;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size) override;
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId) override;

private:
    int m_foundTimes = 0;
};

class AdbTaskLaunchActivity : public AdbTask {
public:
    using AdbTask::AdbTask;
    virtual void cleanup() override;
    virtual bool start() override;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size) override;
    virtual Res onError(AdbContext::FStream fstream) override;
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId) override;
private:
    virtual void createIntentParams(std::list<std::string> &cl) = 0;
};

class AdbTaskRunConnect : public AdbTaskLaunchActivity {
public:
    using AdbTaskLaunchActivity::AdbTaskLaunchActivity;
private:
    virtual void createIntentParams(std::list<std::string> &cl) override;
};

class AdbTaskRunDisconnect : public AdbTaskLaunchActivity {
public:
    using AdbTaskLaunchActivity::AdbTaskLaunchActivity;
private:
    virtual void createIntentParams(std::list<std::string> &cl) override;
};

class AdbTaskRunLogcat : public AdbTask {
public:
    using AdbTask::AdbTask;
    virtual void cleanup() override;
    virtual bool start() override;
    virtual Res onTimer(AdbContext::FStream fstream, unsigned int timerId) override;
protected:
    Res lookupTag(AdbContext::FStream fstream, const char *input, std::size_t &size);
private:
    virtual Res onTagLine(std::string_view &line) = 0;
};

class AdbTaskWaitConnectLog : public AdbTaskRunLogcat {
public:
    using AdbTaskRunLogcat::AdbTaskRunLogcat;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size) override;
    virtual Res onTagLine(std::string_view &line) override;
};

class AdbTaskWaitDisconnectLog : public AdbTaskRunLogcat {
public:
    using AdbTaskRunLogcat::AdbTaskRunLogcat;
    virtual Res onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size) override;
private:
    virtual Res onTagLine(std::string_view &line) override;
};


#endif // ADBTASK_H
