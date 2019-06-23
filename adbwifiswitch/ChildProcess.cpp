#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <thread>

#include "Logger.h"
#include "ChildProcess.h"

namespace {
enum {
    SIDE_READ = 0,
    SIDE_WRITE = 1,

    PIPE_STDIN = 0,
    PIPE_STDOUT = 1,
    PIPE_STDERR = 2
};
}

ChildProcess::ChildProcess(int flags)
    : m_flags(flags), m_pid(-1)
{

}

ChildProcess::~ChildProcess()
{
    cleanup( true );
}

void ChildProcess::cleanup(bool force_stop, int signal)
{
    for(auto fd : {&m_fdStdin, &m_fdStdout, &m_fdStderr}) {
        if (*fd < 0) {
            close( *fd );
            *fd = -1;
        }
    }
    if (m_pid > 0) {
        wait( force_stop, signal );
    }
}

bool ChildProcess::exec(const std::string &cmd, const std::string cl_params)
{
    std::array<int[2], 3> pipes;
    int child_pid;
    int nResult;
    const bool with_stderr = 0 != (m_flags & Flags::fStdErr);

    if (!openPipe(pipes[PIPE_STDIN], false)) {
        LOGE(true, "Fail create pipe for child input");
        return false;
    }
    std::size_t pcleanup = PIPE_STDIN;

    bool err = !openPipe(pipes[PIPE_STDOUT], true);
    if (err) {
        LOGE(true, "Fail create pipe for child output");
    } else {
        pcleanup = PIPE_STDOUT;

        if (with_stderr) {
            err = !openPipe(pipes[PIPE_STDERR], true);
            if (err) LOGE(true, "Fail create pipe for child stderr");
            else pcleanup = PIPE_STDERR;
        }
    }

    if (err) {
        for(std::size_t j=PIPE_STDIN; j<=pcleanup; j++ )
            for(auto i = 0; i < 2; i++) close( pipes[j][i] );
        return false;
    }

    child_pid = fork();
    if (0 == child_pid) {
        // child process

        // redirect stdin
        if (dup2(pipes[PIPE_STDIN][SIDE_READ], STDIN_FILENO) == -1) {
            exit(errno);
        }

        // redirect stdout
        if (dup2(pipes[PIPE_STDOUT][SIDE_WRITE], STDOUT_FILENO) == -1) {
            exit(errno);
        }

        // redirect stderr
        if (dup2(with_stderr ? pipes[PIPE_STDERR][SIDE_WRITE] : pipes[PIPE_STDOUT][SIDE_WRITE], STDERR_FILENO) == -1) {
            exit(errno);
        }

        for(std::size_t j=PIPE_STDIN; j <= (with_stderr ? PIPE_STDERR : PIPE_STDOUT); j++ )
            for(auto i = 0; i < 2; i++) close( pipes[j][i] );

        const int sberr = setvbuf( stdout, nullptr, _IONBF, 0 );
        LOGE(sberr != 0, "setbuf() failed: %d errno %d", sberr, errno);

        nResult = execlp(cmd.c_str(), cmd.c_str(), cl_params.c_str(), nullptr);

        LOGE(true, "Exec() error. Result %d ", nResult);

        exit(nResult);

    } else if (child_pid > 0) {
        // parent continues here
        m_pid = child_pid;

        // close unused file descriptors, these are for child only
        close(pipes[PIPE_STDIN][SIDE_READ]);
        close(pipes[PIPE_STDOUT][SIDE_WRITE]);

        m_fdStdin = pipes[PIPE_STDIN][SIDE_WRITE];
        m_fdStdout = pipes[PIPE_STDOUT][SIDE_READ];

        if (with_stderr) {
            close(pipes[PIPE_STDERR][SIDE_WRITE]);
            m_fdStderr = pipes[PIPE_STDERR][SIDE_READ];
        }

        return true;
    } else {
        // failed to create child
        for(std::size_t j=PIPE_STDIN; j <= (with_stderr ? PIPE_STDERR : PIPE_STDOUT); j++ )
            for(auto i = 0; i < 2; i++) close( pipes[j][i] );
    }
    return false;
}


// class ChildProcess:: private methods

bool ChildProcess::openPipe(int *fdpair, bool nb_read_side)
{
    if (pipe(fdpair) < 0) return false;
    if (m_flags & Flags::fNonblock) {
        if (fcntl(fdpair[nb_read_side ? 0 : 1], F_SETFL, O_NONBLOCK) < 0) {
            LOGE(true, "Fcntl fail side read=%d", nb_read_side?1:0);
            close( fdpair[0] );
            close( fdpair[1] );
            return false;
        }
    }
    return true;
}

bool ChildProcess::wait(bool force_stop, int signal)
{
    if (m_pid <= 0) {
        LOGD(true, "wait(): not running");
        return true;
    }

    bool process_found = true;
    if (force_stop) {
        if (!signal) signal = SIGTERM;
        if (kill( m_pid, signal ) < 0) {
            if (errno == ESRCH) process_found = false;
        }
    }
    int wstatus, ret = 0;
    for (int i=0; i<3; i++) {
        ret = waitpid( m_pid, &wstatus, WNOHANG );
        if (ret > 0) {
            assert( ret == m_pid );
            break;
        } else if (ret == 0) {
            LOGD(true, "wait for 1 second...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            assert( errno != EINVAL );
            LOGD(true, "waitpid() ret %d", ret);
            break;
        }
    }
    if (ret < 0) {
        if (errno == ECHILD) {
            LOGD(true, "No child %d or ignoring SIGCHLD", m_pid);
        } else {
            LOGD(true, "Got errno %d after waitpid() for pid=%d", errno, m_pid);
        }
    } else if (ret == 0) {
        if (process_found) {
            if (signal != SIGKILL) {
                LOGD(true, "Killing pid %d", m_pid);
                kill( m_pid, SIGKILL );
            }
            LOGD(true, "Final wait");
            ret = waitpid( m_pid, &wstatus, 0 );
            LOGD(ret != m_pid, "After waitpid() pid=%d ret=%d errno=%d", m_pid, ret, errno);
        } else {
            LOGD(true, "Fail to kill && can't waitpid()");
        }
    }

    const bool process_stopped = ret == m_pid;
    if (process_stopped) {
        if (WIFEXITED(wstatus)) {
            LOGD(true, "Chaild process %d exited normally. Exit status %d", m_pid, WEXITSTATUS(wstatus));
        } else if (WIFSIGNALED(wstatus)) {
            LOGD(true, "Chaild process %d exited by signal %d%s.", m_pid, WTERMSIG(wstatus),
                 WCOREDUMP(wstatus) ? " with core dump" : "");
        } else {
            LOGD(true, "Shouldn't be");
        }
    }
    m_pid = 0;
    return process_stopped;
}
