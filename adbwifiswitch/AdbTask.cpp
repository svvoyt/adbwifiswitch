
#include <cassert>
#include <string_view>

#include "Logger.h"

#include "AdbTask.h"

namespace {
enum {
    FirstPromptWaitTime = 10, // seconds
    SecondPromptWaitTime = 3, // seconds
    TaskTimerId=10,
};
const char LineFeed[] = "\n";
const char ExitCmd[] = "\nexit\n";
} // namespace anonymous

AdbTask::AdbTask(std::shared_ptr<AdbContext> ctx)
    : m_context(std::move(ctx))
{
    
}

AdbTask::~AdbTask() = default;

AdbTask::Res AdbTask::onDataReady(AdbContext::FStream fstream, const char *input, std::size_t &size)
{
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
