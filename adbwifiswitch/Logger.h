#ifndef LOGGER_H
#define LOGGER_H

#include <syslog.h>
#include <string>

class Logger
{
public:
    Logger(const std::string &tag = std::string());

    static Logger &instance();

    void log( int prio, const char *format, ... );

    template <typename T, typename... Args>
    typename std::enable_if<std::is_same<std::string, typename std::decay<T>::type>::value, int>::type
    log( int prio, T &format, Args&&... args) {
        log(prio, format.c_str(), std::forward<Args>(args)...);
        return 0;
    }

    void setTag(const std::string &tag);
    void updatePid();
    void verbose(bool enable);

private:
    int m_maxPrio;
    std::string m_tag;
    std::size_t m_tagLen;
    std::string m_pid;
    std::size_t m_pidLen;
};


#define LOG_GEN(c, prio, format, ...) \
    do { \
        if (c) Logger::instance().log(prio, format, ##__VA_ARGS__); \
    } while(0)

#define LOGE(c, format, ...) LOG_GEN( (c), LOG_ERR, format, ##__VA_ARGS__)
#define LOGW(c, format, ...) LOG_GEN( (c), LOG_WARNING, format, ##__VA_ARGS__)
#define LOGI(c, format, ...) LOG_GEN( (c), LOG_INFO, format, ##__VA_ARGS__)
#define LOGD(c, format, ...) LOG_GEN( (c), LOG_DEBUG, format, ##__VA_ARGS__)
#define LOG(c, format, ...) LOGE( (c), format, ##__VA_ARGS__)

#endif // LOGGER_H
