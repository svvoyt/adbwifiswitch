cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(adbwifiswitch)

SET( SRCS_LIST
AdbContext.cpp
AdbController.cpp
AdbTask.cpp
Buffers.cpp
ChildProcess.cpp
Config.cpp
FileHandler.cpp
FilePoller.cpp
Logger.cpp
main.cpp
)

SET( HDRS_LIST
AdbContext.h
AdbController.h
AdbTask.h
Buffers.h
ChildProcess.h
Config.h
FileHandler.h
FilePoller.h
Logger.h
)


add_executable(${PROJECT_NAME}
    ${SRCS_LIST}
    ${HDRS_LIST}
    )

set_target_properties(adbwifiswitch PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
)

