#include "unistd.h"

#include <array>
#include <cassert>

#include "AdbController.h"
#include "Config.h"
#include "Logger.h"


namespace {

class StaticContextDeleter {
public:
    void operator()(AdbContext *) {}
};

class ConnectScript : public Script {
public:
    using Script::Script;
    
    virtual std::shared_ptr<AdbTask> getNextTask() override
    {
        assert( hasNext() );
        if (m_curr < 0) m_curr = 0; else m_curr++;
        switch (m_curr) {
            case TaskDef::WaitPrompt:
            case TaskDef::LaunchActivity:
            case TaskDef::GetResult:
            default:
                assert(false);
        }
        return std::shared_ptr<AdbTask>();
    }

    virtual bool hasNext() const override {
        return m_curr < (TaskCount-1);
    }
    
    
private:
    enum TaskDef {
        WaitPrompt,
        LaunchActivity,
        GetResult,
        TaskCount
    };
    
};
}


AdbController::AdbController(std::shared_ptr<Config> cfg, FilePoller &fpoll)
    : m_adbCtx(cfg), m_adbProc(ChildProcess::Flags::fDefault | ChildProcess::Flags::fStdErr), 
      m_fpoll(fpoll)
{
    
}

bool AdbController::connectWiFi()
{
    LOGD(true, "connectWiFi()");
    
    if (!init()) return false;
    m_script = std::make_shared<ConnectScript>( std::shared_ptr<AdbContext>(&m_adbCtx, StaticContextDeleter() ) );
    return true;
}


// AdbController:: private members

void AdbController::cleanup()
{
    m_currTask.reset();
    m_script.reset();

    foreachFh( [&](AdbController::FHCommon *fptr) { fptr->setState( false ); } );

    m_adbProc.cleanup();
    
    foreachFh( [&](AdbController::FHCommon *fptr) {
        if (fptr->handlerId != FilePoller::BadHandlerId)
            m_fpoll.removeHandler( fptr->handlerId );
    });
    
    m_adbStdin.reset();
    m_adbStdout.reset();
    m_adbStderr.reset();
}

inline void AdbController::foreachFh(std::function<void(AdbController::FHCommon *)> proc)
{
    for(auto fptr : {static_cast<FHCommon *>(m_adbStdin.get()),
                     static_cast<FHCommon *>(m_adbStdout.get()),
                     static_cast<FHCommon *>(m_adbStderr.get())} ) {
        if (fptr) proc( fptr );
    }
}

bool AdbController::init()
{
    if (!m_adbProc.exec( m_adbCtx.config()->getAdbCmd(), "" )) {
        LOG(true, "Can't spawn %s", m_adbCtx.config()->getAdbCmd().c_str());
        return false;
    }
    
    m_adbStdin = std::make_shared<FHStdIn>(*this, m_adbProc.getStdinFd());
    m_adbStdout = std::make_shared<FHStdOut>(*this, m_adbProc.getStdoutFd());
    m_adbStderr = std::make_shared<FHStdErr>(*this, m_adbProc.getStderrFd());
    
    m_adbStdin->handlerId = m_fpoll.addHandler( m_adbStdin );
    const bool stdin_added = m_adbStdin->handlerId != FilePoller::BadHandlerId;
    
    m_adbStdout->handlerId = stdin_added ? m_fpoll.addHandler( m_adbStdout ) : FilePoller::BadHandlerId;
    const bool stdout_added =  m_adbStdout->handlerId != FilePoller::BadHandlerId;
    
    m_adbStderr->handlerId = stdout_added ? m_fpoll.addHandler( m_adbStderr ) : FilePoller::BadHandlerId;
    const bool stderr_added =  m_adbStderr->handlerId != FilePoller::BadHandlerId;
    
    if (!stdin_added || !stdout_added || !stderr_added) {
        LOG(true, "Fail to register for polling: in:%d out:%d err:%d",
            stdin_added?1:0, stdout_added?1:0, stderr_added?1:0);
        cleanup();
        return false;
    }
    
    return true;
}

bool AdbController::switchTask(AdbTask::Res res)
{
    switch (res) {
        default:
            assert( false );
        case AdbTask::Res::Fail:
            LOGI(true, "Execution failed");
            cleanup();
            return false;

        case AdbTask::Res::Continue:
            return true;

        case AdbTask::Res::Next:
            break;
    }
    
    if (m_script->hasNext()) {
        m_currTask = m_script->getNextTask();
        assert(m_currTask);
    } else 
        cleanup();
    
    return true;
}


// AdbController::FHCommon class implementation

AdbController::FHCommon::FHCommon(AdbController &owner, int fd)
    : FileHandler( fd ), m_owner(owner)
{
    
}

long AdbController::FHCommon::Read(std::size_t max)
{
    std::size_t total = 0;
    while (total < max) {
        std::size_t rest = m_readBuf.restSize();
        if (rest == 0) {
            rest = (((2*total) >> 10) + 1) << 10;
        }
        long ret = read(getFd(), m_readBuf.readPtr(), rest );
        if (ret < 0) {
            if (errno == EAGAIN) break;
            LOG(true, "Read error occured, Errno %d", errno);
            return -1;
        }
        const std::size_t sz = static_cast<std::size_t>( ret );
        total += sz;
        m_readBuf.addFilled( sz );
        if (sz < rest) break;
    }
    return static_cast<long>( total );
}

long AdbController::FHCommon::Write(const void *ptr, std::size_t size)
{
    long ret = write( getFd(), ptr, size );
    if (ret < 0) {
        if (errno == EAGAIN) return 0;
        LOG(true, "Write error occured, Errno %d", errno);
    }
    return ret;
}

bool AdbController::FHCommon::readData(ReadBuffer &rbuf, AdbTask::HandlerType handler)
{
    auto read_sz = Read();
    if (read_sz > 0) {
#ifndef NDEBUG
        std::string s(m_readBuf.head(), m_readBuf.filledSize());
        LOGD(true, "Read str: %s", s.c_str());
#endif
    } else {
        LOG(read_sz == 0, "zero read");
        return m_owner.switchTask( AdbTask::Res::Fail );
    }

    assert(!rbuf.empty());
    std::size_t sz = rbuf.filledSize();
    std::string out;
    auto res = handler( rbuf.head(), sz, out );
    if (res != AdbTask::Res::Fail) {
        rbuf.cut( sz );
        if (!out.empty()) {
            if (!m_owner.m_adbStdin->put( out )) res = AdbTask::Res::Fail;
        }
    }
    return m_owner.switchTask( res );
}

bool AdbController::FHStdIn::onTimer(unsigned int timerId)
{
    LOGD(true, "stdin onTimer() %u", timerId);
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdIn::onReadyToRead()
{
    LOGI(true, "Adb's stdin: onReadyToRead()");
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdIn::onReadyToWrite()
{
    bool empty = m_writeBuf.empty();
    if (!empty) {
        auto sz = Write( m_writeBuf.ptr(), m_writeBuf.size() );
        if (sz < 0) {
            LOG(true, "Write fail");
            return m_owner.switchTask( AdbTask::Res::Fail );
        }
        m_writeBuf.pushHead( static_cast<std::size_t>( sz ) );
        empty = m_writeBuf.empty();
    }
    if (empty) setWriteRequest( false );
    return true;
}

bool AdbController::FHStdIn::onError()
{
    LOG(true, "Error condition on stdin");
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdIn::put(const std::string &data)
{
    m_writeBuf.append( data.data(), data.size() );
    setWriteRequest( true );
    return true;
}

bool AdbController::FHStdOut::onTimer(unsigned int timerId)
{
    LOGD(true, "stdout onTimer() %u", timerId);
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdOut::onReadyToRead()
{
    LOGD(true, "FHStdOut::onReadyToRead()");
    return readData( m_readBuf, std::bind(&AdbTask::onStderrData, m_owner.m_currTask.get(),
                                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) );
}

bool AdbController::FHStdOut::onReadyToWrite()
{
    assert( false );
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdOut::onError()
{
    LOG(true, "Error condition on stdout");
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdErr::onTimer(unsigned int timerId)
{
    LOGD(true, "stderr onTimer() %u", timerId);
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdErr::onReadyToRead()
{
    LOGD(true, "FHStdErr::onReadyToRead()");
    return readData( m_readBuf, std::bind(&AdbTask::onStderrData, m_owner.m_currTask.get(),
                                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) );
}

bool AdbController::FHStdErr::onReadyToWrite()
{
    assert( false );
    return m_owner.switchTask( AdbTask::Res::Fail );
}

bool AdbController::FHStdErr::onError()
{
    LOG(true, "Error condition on stderr");
    return m_owner.switchTask( AdbTask::Res::Fail );
}
