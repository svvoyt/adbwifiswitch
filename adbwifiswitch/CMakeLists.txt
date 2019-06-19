cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(adbwifiswitch)

SET(CXX_STANDARD 11)
SET(CXX_STANDARD_REQUIRED ON)

SET( SRCS_LIST
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
