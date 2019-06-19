#ifndef ADBCONTROLLER_H
#define ADBCONTROLLER_H

#include <functional>
#include <memory>

#include "Buffers.h"
#include "ChildProcess.h"
#include "FileHandler.h"
#include "FilePoller.h"

class Config;

class AdbTaskContext {
public:
    bool wrStdin(const void *buf , std::size_t size);
    bool prStdin(const char *format, ...);
    
private:
    std::shared_ptr<FileHandler> m_adbStdin, m_adbStdout, m_adbStderr;
    ChildProcess &m_adbProc;
};

class AdbTask {
public:
    enum Res {
        Next, Continue, Fail
    };
    
    AdbTask( AdbTaskContext &ctx );
    virtual Res onStderrData();
    virtual Res onStdoutData();
};

class AdbScenario {
public:
    virtual bool start() = 0;
};

class AdbController
{
public:
    AdbController(std::shared_ptr<Config> cfg, FilePoller &fpoll);

    
    
private:
    enum Mode {
        ConnectWiFi,
        DisconnectWiFi
    };
    
    enum Task {
        DetectPrompt,
        LaunchWiFi,
        
    };
    
    class FHCommon : public FileHandler {
    public:
        FHCommon(AdbController &owner, int fd);

        long Read(std::size_t max = 10*1024);
        
        FilePoller::HandlerId handlerId;

    protected:
        ReadBuffer m_readBuf;
        AdbController &m_owner;
    };
    
    class FHStdIn : public FHCommon {
    public:
        using FHCommon::FHCommon;

        virtual bool onTimer( unsigned int timerId ) override;
        virtual bool onReadyToRead() override;
        virtual bool onReadyToWrite() override;
        virtual bool onError() override;
    };
    
    class FHStdOut : public FHCommon {
    public:
        using FHCommon::FHCommon;

        virtual bool onTimer( unsigned int timerId ) override;
        virtual bool onReadyToRead() override;
        virtual bool onReadyToWrite() override;
        virtual bool onError() override;
    };
    
    class FHStdErr : public FHCommon {
    public:
        using FHCommon::FHCommon;

        virtual bool onTimer( unsigned int timerId ) override;
        virtual bool onReadyToRead() override;
        virtual bool onReadyToWrite() override;
        virtual bool onError() override;
    };
    
    void cleanup();
    void foreachFh( std::function<void(FHCommon *)> proc );
    bool init();
    
    ChildProcess m_adbProc;
    std::shared_ptr<FHStdIn> m_adbStdin;
    std::shared_ptr<FHStdOut> m_adbStdout;
    std::shared_ptr<FHStdErr> m_adbStderr;
    std::shared_ptr<Config> m_cfg;
    FilePoller &m_fpoll;
};

#endif // ADBCONTROLLER_H
