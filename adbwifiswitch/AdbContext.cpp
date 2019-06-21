
#include "AdbContext.h"

AdbContext::AdbContext(std::shared_ptr<Config> cfg)
    : m_config(std::move(cfg))
{
}

AdbContext::~AdbContext() = default;

