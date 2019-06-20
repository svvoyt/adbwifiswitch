#ifndef ADBCONTROLLER_H
#define ADBCONTROLLER_H

#include <functional>
#include <memory>

#include "AdbContext.h"
#include "AdbTask.h"
#include "Buffers.h"
#include "ChildProcess.h"
#include "FileHandler.h"
#include "FilePoller.h"

class Config;
class Script;

class AdbController
{
public:
    AdbController(std::shared_ptr<Config> cfg, FilePoller &fpoll);

    bool connectWiFi();
    bool disconnectWiFi();
    
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

        FilePoller::HandlerId handlerId;
        
    protected:
        long Read(std::size_t max = 10*1024);
        long Write(const void *ptr, std::size_t size);
        
        bool readData(ReadBuffer &rbuf, AdbTask::HandlerType handler);
        
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

        bool put(const std::string &data);
        
    private:
        WriteBuffer m_writeBuf;
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
    bool switchTask( AdbTask::Res res );
    
    AdbContext m_adbCtx;
    ChildProcess m_adbProc;
    std::shared_ptr<FHStdIn> m_adbStdin;
    std::shared_ptr<FHStdOut> m_adbStdout;
    std::shared_ptr<FHStdErr> m_adbStderr;
    std::shared_ptr<AdbTask> m_currTask;
    FilePoller &m_fpoll;
    std::shared_ptr<Script> m_script;
};

class Script {
public:
    Script(std::shared_ptr<AdbContext> ctx) : m_ctx(std::move(ctx)) {}
    virtual ~Script() = default;
    
    std::shared_ptr<AdbContext> ctx() {return m_ctx;}

    virtual std::shared_ptr<AdbTask> getNextTask() = 0;
    virtual bool hasNext() const = 0;
    virtual void reset() {m_curr = -1;}

protected:
    int m_curr = -1;
private:
    std::shared_ptr<AdbContext> m_ctx;
};


#endif // ADBCONTROLLER_H
