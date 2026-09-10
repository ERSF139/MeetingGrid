cmake_minimum_required(VERSION 3.16)

project(MeetingGridServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# MSVC 下强制源码按 UTF-8 解释，保证中文字符串正确
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets)

qt_add_executable(MeetingGridServer
    main.cpp
    protocol.h
    SignalingServer.h
    SignalingServer.cpp
)

target_link_libraries(MeetingGridServer PRIVATE Qt6::Core Qt6::Network Qt6::WebSockets)
