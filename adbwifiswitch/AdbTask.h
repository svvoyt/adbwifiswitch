#ifndef ADBTASK_H
#define ADBTASK_H

#include <functional>
#include <memory>
#include <string>

class AdbContext;


class AdbTask {
public:
    enum Res {
        Next, Continue, Fail
    };
    
    typedef std::function<Res(const char *, std::size_t &, std::string &)> HandlerType;
    
    AdbTask( std::shared_ptr<AdbContext> ctx );
    virtual ~AdbTask();
    
    virtual Res onStderrData(const char *input, std::size_t &size, std::string &out);
    virtual Res onStdoutData(const char *input, std::size_t &size, std::string &out);

private:
    std::shared_ptr<AdbContext> m_context;
};

#endif // ADBTASK_H
