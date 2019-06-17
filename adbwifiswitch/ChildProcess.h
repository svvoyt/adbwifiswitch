#ifndef CHILDPROCESS_H
#define CHILDPROCESS_H

#include <string>

class ChildProcess
{
public:
    enum Flags {
        fStdin  = 0x1,
        fStdout = 0x2,
        fStdErr = 0x4,
        fNonblock = 0x8,

        fDefaultStream = fStdout | fStdin,
        fDefault = fDefaultStream | fNonblock
    };

    ChildProcess(int flags = Flags::fDefault);
    ~ChildProcess();

    void cleanup(bool force_stop = false, int signal = 0);
    bool exec(const std::string &cmd, const std::string cl_params);

    int getStdinFd() const {return m_fdStdin;}
    int getStdoutFd() const {return m_fdStdout;}
    int getStderrFd() const {return m_fdStderr;}
    bool wait(bool force_stop = false, int signal = 0);

private:

    bool openPipe( int *fdpair, bool nb_read_side );

    int m_fdStdin = -1;
    int m_fdStdout = -1;
    int m_fdStderr = -1;
    int m_flags;
    int m_pid;
};

#endif // CHILDPROCESS_H
