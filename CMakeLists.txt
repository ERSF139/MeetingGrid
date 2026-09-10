cmake_minimum_required(VERSION 3.16)

project(MeetingGrid VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# MSVC 下强制源码按 UTF-8 解释，保证中文字符串正确
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia WebSockets)

qt_add_executable(MeetingGrid WIN32
    src/main.cpp
    src/GridManager.h    src/GridManager.cpp
    src/IconFactory.h    src/IconFactory.cpp
    src/VideoTile.h      src/VideoTile.cpp
    src/VideoSource.h    src/VideoSource.cpp
    src/AudioSource.h    src/AudioSource.cpp
    src/ControlButton.h  src/ControlButton.cpp
    src/GridContainer.h  src/GridContainer.cpp
    src/MainWindow.h     src/MainWindow.cpp
    src/SignalingClient.h src/SignalingClient.cpp
)

target_include_directories(MeetingGrid PRIVATE server)
target_link_libraries(MeetingGrid PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::WebSockets)

# ---------------------------------------------------------------------------
# 单元测试
# ---------------------------------------------------------------------------
enable_testing()
find_package(Qt6 QUIET COMPONENTS Test)
if(Qt6Test_FOUND)
    qt_add_executable(gridtest
        tests/tst_gridmanager.cpp
        src/GridManager.h
        src/GridManager.cpp
    )
    target_include_directories(gridtest PRIVATE src)
    target_link_libraries(gridtest PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME gridtest COMMAND gridtest)

    # 信令集成测试：进程内直连 SignalingServer + SignalingClient
    qt_add_executable(signalingtest
        tests/tst_signalingclient.cpp
        src/SignalingClient.h    src/SignalingClient.cpp
        server/SignalingServer.h server/SignalingServer.cpp
        server/protocol.h
    )
    target_include_directories(signalingtest PRIVATE src server)
    target_link_libraries(signalingtest PRIVATE Qt6::Test Qt6::Core Qt6::Network Qt6::WebSockets)
    add_test(NAME signalingtest COMMAND signalingtest)
endif()

# 信令服务端

option(BUILD_SERVER "Build the WebSocket signaling server" ON)
if(BUILD_SERVER)
    add_subdirectory(server)
endif()
