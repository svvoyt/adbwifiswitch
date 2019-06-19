#include "AdbTask.h"

//AdbTask::AdbTask()
//{

//}

AdbTask::Res AdbTask::onStderrData()
{
    return Res::Fail;
}

AdbTask::Res AdbTask::onStdoutData()
{
    return Res::Fail;
}

