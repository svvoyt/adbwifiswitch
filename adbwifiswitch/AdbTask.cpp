#include "AdbTask.h"

AdbTask::AdbTask(std::shared_ptr<AdbContext> ctx)
    : m_context(std::move(ctx))
{
    
}

AdbTask::~AdbTask() = default;

AdbTask::Res AdbTask::onStderrData(const char *, std::size_t &, std::string &)
{
    return Res::Fail;
}

AdbTask::Res AdbTask::onStdoutData(const char *, std::size_t &, std::string &)
{
    return Res::Fail;
}


