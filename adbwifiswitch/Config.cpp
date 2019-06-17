#include "Config.h"

// Config::Builder class implementation

Config Config::Builder::build() const
{
    Config ret;
    *static_cast<ConfigData *>(&ret) = *this;
    return ret;
}
