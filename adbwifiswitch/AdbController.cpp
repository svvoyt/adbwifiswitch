#include "unistd.h"

#include <cassert>

#include "AdbController.h"
#include "Config.h"
#include "Logger.h"

AdbController::AdbController(std::shared_ptr<Config> cfg, FilePoller &fpoll)
    : m_adbProc(ChildProcess::Flags::fDefault | ChildProcess::Flags::fStdErr), 
      m_cfg(cfg), m_fpoll(fpoll)
{
    
}


// AdbController:: private members

void AdbController::cleanup()
{
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
    if (!m_adbProc.exec( m_cfg->getAdbCmd(), "" )) {
        LOG(true, "Can't spawn %s", m_cfg->getAdbCmd().c_str());
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

bool AdbController::FHStdIn::onTimer(unsigned int timerId)
{
    LOGD(true, "stdin onTimer() %u", timerId);
}

bool AdbController::FHStdIn::onReadyToRead()
{
    LOGI(true, "Adb's stdin: onReadyToRead()");
    m_owner.cleanup();
    return false;
}

bool AdbController::FHStdIn::onReadyToWrite()
{
    
}

bool AdbController::FHStdIn::onError()
{
    LOG(true, "Error condition on stdin");
    m_owner.cleanup();
}

bool AdbController::FHStdOut::onTimer(unsigned int timerId)
{
    LOGD(true, "stdout onTimer() %u", timerId);
}

bool AdbController::FHStdOut::onReadyToRead()
{
    
}

bool AdbController::FHStdOut::onReadyToWrite()
{
    assert( false );
}

bool AdbController::FHStdOut::onError()
{
    LOG(true, "Error condition on stdout");
    m_owner.cleanup();
}

bool AdbController::FHStdErr::onTimer(unsigned int timerId)
{
    LOGD(true, "stderr onTimer() %u", timerId);
}

bool AdbController::FHStdErr::onReadyToRead()
{
    auto sz = Read();
    if (sz > 0) {
        std::string s(m_readBuf.head(), m_readBuf.filledSize());
        LOGI(true, "Adb's stderr: %s", s.c_str());
        m_readBuf.cut();
    } else if (sz == 0) {
        LOGI(true, "Adb's stderr: zero read");
    } else {
        m_owner.cleanup();
        return false;
    }
    return true;
}

bool AdbController::FHStdErr::onReadyToWrite()
{
    assert( false );
}

bool AdbController::FHStdErr::onError()
{
    LOG(true, "Error condition on stderr");
    m_owner.cleanup();
    
}
