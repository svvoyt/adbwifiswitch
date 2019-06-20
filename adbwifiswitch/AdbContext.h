#ifndef ADBCONTEXT_H
#define ADBCONTEXT_H

#include <memory>


class Config;

class AdbContext
{
public:
    AdbContext(std::shared_ptr<Config> cfg);

    Config *config() {return m_config.get();}
    const Config *config() const {return m_config.get();}
    std::shared_ptr<Config> configShared() {return m_config;}
    
private:
    std::shared_ptr<Config> m_config;
};

#endif // ADBCONTEXT_H
