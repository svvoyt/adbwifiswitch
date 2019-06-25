#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct ConfigData {
    std::string adbCmd;
    std::string authType;
    std::string password;
    std::string ssid;
    std::string uniqTag;
};

class Config : ConfigData {
public:

    class Builder :  ConfigData {
    public:
        Builder &setAdbCmd(const std::string &cmd) {adbCmd.assign( cmd ); return *this;}
        Builder &setAuthType(const std::string &atype) {authType.assign( atype ); return *this;}
        Builder &setPassword(const std::string &pwd) {password.assign( pwd ); return *this;}
        Builder &setSsid(const std::string &_ssid) {ssid.assign( _ssid ); return *this;}
        Config build() const;
    };

    const std::string &getAdbCmd() const {return adbCmd;}
    const std::string &getAuthType() const {return authType;}
    const std::string &getPassword() const {return password;}
    const std::string &getSsid() const {return ssid;}
    const std::string &getUniqTag() const {return uniqTag;}
    
    std::string to_string() const;
};

#endif // CONFIG_H
