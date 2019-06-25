
#include <chrono>
#include <sstream>

#include "Config.h"

// Config::Builder class implementation

Config Config::Builder::build() const
{
    Config ret;

    *static_cast<ConfigData *>(&ret) = *this;

    auto time = std::chrono::time_point_cast<std::chrono::milliseconds>( std::chrono::system_clock::now() )
                    .time_since_epoch().count();
    while(time != 0) {
        ret.uniqTag.append( std::to_string( time&0xff ) );
        time >>= 8;
    }

    return ret;
}

std::string Config::to_string() const
{
    std::stringstream ss;
    ss << "Adb " << getAdbCmd() << " ssid " << getSsid() << " key " << getPassword()
       << " auth type " << getAuthType() << " uniq " << getUniqTag();
    return ss.str();
}
