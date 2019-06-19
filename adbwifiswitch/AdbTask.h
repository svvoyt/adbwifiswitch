#ifndef ADBTASK_H
#define ADBTASK_H

#include <memory>

class FileHandler;
class AdbTask;


class AdbTask {
public:
    enum Res {
        Next, Continue, Fail
    };
    
//    AdbTask( AdbTaskContext &ctx );
    virtual Res onStderrData();
    virtual Res onStdoutData();
};

#endif // ADBTASK_H
