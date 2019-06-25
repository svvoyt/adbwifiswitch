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
    ~AdbController();

    bool connectWiFi();
    bool disconnectWiFi();
    int exitCode() const;
    
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

        virtual AdbContext::FStream getFH() const = 0;

        virtual bool onError() override;
        virtual bool onReadyToRead() override;
        virtual bool onReadyToWrite() override;
        virtual bool onTimer( unsigned int timerId ) override;

        FilePoller::HandlerId handlerId;
        
    protected:
        long Read(std::size_t max = 10*1024);
        long Write(const void *ptr, std::size_t size);
        
        ReadBuffer m_readBuf;
        AdbController &m_owner;
    };
    
    class FHStdIn : public FHCommon {
    public:
        using FHCommon::FHCommon;

        virtual bool onReadyToRead() override;
        virtual bool onReadyToWrite() override;
        virtual AdbContext::FStream getFH() const override;

        bool put(const std::string &data);
        bool put(const void *, std::size_t size);
        
    private:
        WriteBuffer m_writeBuf;
    };
    
    class FHStdOut : public FHCommon {
    public:
        using FHCommon::FHCommon;
        virtual AdbContext::FStream getFH() const override;
    };
    
    class FHStdErr : public FHCommon {
    public:
        using FHCommon::FHCommon;
        virtual AdbContext::FStream getFH() const override;
    };
    
    class Context : public AdbContext {
    public:
        Context(AdbController &owner, std::shared_ptr<Config> cfg);

        virtual bool startAdb(const std::list<std::string> &cl) override;
        virtual void stopAdb() override;
        virtual bool writeStdIn(const void *buf, std::size_t size) override;
        virtual bool timerCtl(FStream fstream, unsigned int timerId, bool start,
                              std::chrono::milliseconds ms=std::chrono::milliseconds::zero()) override;
    private:
        AdbController &m_owner;
    };
    
    void cleanup();
    void cleanupChildProc();
    void foreachFh( std::function<void(FHCommon *)> proc );
    FileHandler *getFH(AdbContext::FStream fstream);
    bool initAdb(const std::list<std::string> &cl_params);
    bool switchTask( AdbTask::Res res );
    
    Context m_adbCtx;
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
