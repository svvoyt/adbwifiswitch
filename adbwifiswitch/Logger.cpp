#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include "Logger.h"

Logger::Logger(const std::string &tag)
    : m_maxPrio(LOG_INFO), m_tagLen(0), m_pidLen(0)
{
    setTag(tag);
}

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

void Logger::log(int prio, const char *format, ... )
{
    if (prio > m_maxPrio) return;

    const auto format_len = strlen( format );
    const auto tag_len = m_tagLen;
    const auto pid_len = m_pidLen;
    const auto added_size = format_len + tag_len + pid_len + 2;
    char * const fcopy = reinterpret_cast<char *>( alloca( added_size ) );
    char *p = fcopy;

    strcpy(p, m_tag.c_str());
    p += tag_len;
    strcpy(p, m_pid.c_str());
    p += pid_len;
    strcpy(p, format);
    p += format_len;
    strcpy(p, "\n");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, fcopy, ap);
    va_end(ap);

#pragma clang diagnostic pop
}

void Logger::setTag(const std::string &tag)
{
    m_tag.assign( tag );
    m_tagLen = tag.size();
    updatePid();
}

void Logger::updatePid()
{
    m_pid.assign(std::to_string( getpid() ));
    m_pid.insert(m_pid.begin(), '(');
    m_pid.append("): ");
    m_pidLen = m_pid.size();
}

void Logger::verbose(bool enable)
{
    m_maxPrio = enable ? LOG_DEBUG : LOG_INFO;
}
