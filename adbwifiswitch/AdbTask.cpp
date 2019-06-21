
#include <string_view>

#include "Logger.h"

#include "AdbTask.h"

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
    m_context->timerCtl(AdbContext::FStream::fsStdIn, 1, false);
}

bool AdbTaskWaitFirstPrompt::start()
{
    if (!m_context->timerCtl(AdbContext::FStream::fsStdIn, 1, true, std::chrono::seconds(5))) {
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
    if (pos == (view.size() - 1)) return Next;
    return Continue;
}

AdbTask::Res AdbTaskWaitFirstPrompt::onTimer(AdbContext::FStream fstream, unsigned int timerId)
{
    LOGI(true, "Wait for first prompt timed out");
    return Fail;
}
