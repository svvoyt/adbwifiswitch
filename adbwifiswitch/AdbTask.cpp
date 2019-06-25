
#include <cassert>
#include <string_view>

#include "Config.h"
#include "Logger.h"

#include "AdbTask.h"

namespace {
enum {
    FirstAdbLaunchWaitTime = 10, // seconds
    FirstPromptWaitTime = 10, // seconds
    LogcatWaitTime = 30,  // seconds
    SecondPromptWaitTime = 3, // seconds
    TaskTimerId=10,
};
const char LineFeed[] = "\n";
const char ExitCmd[] = "\nexit\n";
const char CtrlC[] = "\0x3";
} // namespace anonymous

namespace java {
const char Agent[] = "com.steinwurf.adbjoinwifi";
const char AgentTag[] = "adbjoinwifi";
const char Activity[] = ".MainActivity";
const char ModeParam[] = "mode ";
const char ModeConnect[] = "connect";
const char ModeDisconnect[] = "disconnect";
const char PasswdParam[] = "password ";
const char PasswdTypeParam[] = "password_type ";
const char PasswordTypeWep[] = "WEP";
const char PasswordTypeWpa[] = "WPA";
const char SsidParam[] = "ssid ";
const char UniqParam[] = "uniq ";
const char ActivitySwitch[] = "-n ";
const char ExtraSwitch[] = "-e ";
const char CmdShell[] = "shell";
const char ShellPtyAlloc[] = "-t";
const char CmdActivityManager[] = "am";
const char CmdStart[] = "start";
const char CmdLogcat[] = "logcat";
const char LogcatThreadTime[] = "-v threadtime";
const char LogcatCountParam[] = "-T20";
const char ConnectSignature[] = "Mode connect run completed";
const char DisconnectSignature[] = "Mode disconnect run completed";
}


AdbTask::AdbTask(std::shared_ptr<AdbContext> ctx)
    : m_context(std::move(ctx))
{
    
}

AdbTask::~AdbTask() = default;

AdbTask::Res AdbTask::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    return Res::Fail;
}

AdbTask::Res AdbTask::onError(AdbContext::FStream fstream)
{
    LOG(true, "onError() fs:%d", fstream);
    return Res::Fail;
}

AdbTask::Res AdbTask::onTimer(AdbContext::FStream fstream, unsigned int timerId)
{
    return Res::Fail;
}


// AdbTaskWaitFirstPrompt class implementation

void AdbTaskWaitFirstPrompt::cleanup()
{
    LOGD(true, "AdbTaskWaitFirstPrompt::cleanup()");
    m_context->writeStdIn(ExitCmd, sizeof(ExitCmd)-1);
    m_foundTimes = 0;
    m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, false);
}

bool AdbTaskWaitFirstPrompt::start()
{
    if (!m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, true, std::chrono::seconds(FirstPromptWaitTime))) {
        LDEB(true, "Start timer fail");
        return false;
    }
    return true;
}

AdbTask::Res AdbTaskWaitFirstPrompt::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    if (!isStdout( fstream )) return Continue;
    std::string_view view( input, size );
    auto pos = view.rfind('$');
    if (pos == (view.size() - 1)) {
        if (m_foundTimes++ == 0) {
            LOGD(true, "Found first '$'");
            if (!m_context->writeStdIn(LineFeed, sizeof(LineFeed)-1) ||
                !m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, true, std::chrono::seconds(SecondPromptWaitTime))) {
                LOGD(true, "Write fail");
                cleanup();
                return Fail;
            }
            return Continue;
        } else {
            assert( m_foundTimes == 2 );
            LOGD(true, "Found second '$'");
            return Next;
        }
    }
    return Continue;
}

AdbTask::Res AdbTaskWaitFirstPrompt::onTimer(AdbContext::FStream fstream, unsigned int timerId)
{
    assert( fstream == AdbContext::FStream::fsStdIn );
    assert( timerId == TaskTimerId );

    LOGI(true, "Wait for prompt timed out (%d)", m_foundTimes);
    cleanup();
    return Fail;
}


// AdbTaskRunDisconnect class implementation

void AdbTaskLaunchActivity::cleanup()
{
    LOGD(true, "AdbTaskWaitFirstPrompt::cleanup()");
    if (isRunning()) {
        m_context->writeStdIn(CtrlC, sizeof(CtrlC)-1);
        m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, false);
        setState( State::Stopped );
    }
}

bool AdbTaskLaunchActivity::start()
{
    std::list<std::string> cl;
    createIntentParams( cl );

    if (m_context->startAdb( cl )) {
        setState( State::Running );
        if (!m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, true, std::chrono::seconds(FirstAdbLaunchWaitTime))) {
            LDEB(true, "Start timer fail");
            cleanup();
            return false;
        }
    }
    return true;
}

AdbTask::Res AdbTaskLaunchActivity::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    return size > 0 ? Continue : Next;
}

AdbTask::Res AdbTaskLaunchActivity::onError(AdbContext::FStream fstream)
{
    LOGD(true, "Got onError() stream %d", static_cast<int>(fstream));
    m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, false);
    m_context->stopAdb();
    setState( State::Stopped );
    return Res::Next;
}

AdbTask::Res AdbTaskLaunchActivity::onTimer(AdbContext::FStream fstream, unsigned int timerId)
{
    assert( fstream == AdbContext::FStream::fsStdIn );
    assert( timerId == TaskTimerId );

    LOGI(true, "Adb hangs");
    cleanup();
    return Fail;
}


// AdbTaskRunConnect class implementation

void AdbTaskRunConnect::createIntentParams(std::list<std::string> &cl)
{
    cl.emplace_back(java::CmdShell);
    cl.emplace_back(java::CmdActivityManager);
    cl.emplace_back(java::CmdStart);
    cl.emplace_back(std::string(java::ActivitySwitch).append(java::Agent).append("/").append(java::Activity));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::ModeParam).append(java::ModeConnect));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::UniqParam).append(m_context->config()->getUniqTag()));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::SsidParam).append(m_context->config()->getSsid()));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::PasswdParam).append(m_context->config()->getPassword()));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::PasswdTypeParam).append(m_context->config()->getAuthType()));
}


// AdbTaskRunDisconnect class implementation

void AdbTaskRunDisconnect::createIntentParams(std::list<std::string> &cl)
{
    cl.emplace_back(java::CmdShell);
    cl.emplace_back(java::CmdActivityManager);
    cl.emplace_back(java::CmdStart);
    cl.emplace_back(std::string(java::ActivitySwitch).append(java::Agent).append("/").append(java::Activity));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::ModeParam).append(java::ModeDisconnect));
    cl.emplace_back(std::string(java::ExtraSwitch).append(java::UniqParam).append(m_context->config()->getUniqTag()));
}


// AdbTaskRunLogcat class implementation

void AdbTaskRunLogcat::cleanup()
{
    if (isRunning()) {
        m_context->writeStdIn(CtrlC, sizeof(CtrlC)-1);
        m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, false);
        setState( State::Stopped );
    }
}

bool AdbTaskRunLogcat::start()
{
    std::list<std::string> cl;

    cl.emplace_back(java::CmdShell);
    cl.emplace_back(java::ShellPtyAlloc);
    cl.emplace_back(java::CmdLogcat);
    cl.emplace_back(java::LogcatThreadTime);
    cl.emplace_back(java::LogcatCountParam);

    if (m_context->startAdb( cl )) {
        if (!m_context->timerCtl(AdbContext::FStream::fsStdIn, TaskTimerId, true, std::chrono::seconds(LogcatWaitTime))) {
            LDEB(true, "Start timer fail");
            setState( State::Stopped );
            return false;
        }
    }
    return true;
}

AdbTask::Res AdbTaskRunLogcat::onTimer(AdbContext::FStream fstream, unsigned int timerId)
{
    assert( fstream == AdbContext::FStream::fsStdIn );
    assert( timerId == TaskTimerId );

    LOGI(true, "Operation timed out - no answer from java agent");
    cleanup();
    return Fail;
}

AdbTask::Res AdbTaskRunLogcat::lookupTag(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    if (!isStdout( fstream )) return Continue;
    std::string_view view( input, size );
    std::size_t pos;
    LOGD(true, "On entry size = %zu", size);
    size = 0;
    while ((pos = view.find("\n")) != std::string::npos) {
        size += pos + 1;
        if (pos == 0) {
            view.remove_prefix( 1 );
            continue;
        }
        std::string_view line( view.substr(0, pos) );
        view.remove_prefix( pos+1 );

        if (line.find( java::AgentTag ) != std::string_view::npos) {
            Res ret = onTagLine( line );
            if (ret != Res::Continue) return ret;
        }
    }
    LOGD(true, "On exit size = %zu", size);
    return Continue;
}


// AdbTaskWaitConnectLog class implementation

AdbTask::Res AdbTaskWaitConnectLog::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    
    return fstream == AdbContext::FStream::fsStdOut ? lookupTag( fstream, input, size ) : Res::Continue;
}

AdbTask::Res AdbTaskWaitConnectLog::onTagLine(std::string_view &line)
{
    LOGD(true, std::string(line.cbegin(), line.cend()).c_str() );
    if (line.find(m_context->config()->getUniqTag()) != std::string_view::npos && 
            line.find( java::ConnectSignature ) != std::string_view::npos) {
        LOG(true, "Wifi connected");
        return Res::Next;
    }
    return Res::Continue;
}

AdbTask::Res AdbTaskWaitDisconnectLog::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
    return fstream == AdbContext::FStream::fsStdOut ? lookupTag( fstream, input, size ) : Res::Continue;
}

AdbTask::Res AdbTaskWaitDisconnectLog::onTagLine(std::string_view &line)
{
    LOGD(true, std::string(line.cbegin(), line.cend()).c_str() );
    if (line.find(m_context->config()->getUniqTag()) != std::string_view::npos && 
            line.find( java::DisconnectSignature ) != std::string_view::npos) {
        LOG(true, "Wifi disconnected");
        return Res::Next;
    }
    return Res::Continue;
}
