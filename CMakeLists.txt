# 设置CMake最低支持版本，低于3.16的cmake直接报错终止
cmake_minimum_required(VERSION 3.16)

# 定义项目：项目名log，版本1.0.0；仅使用C++编译器，不启用C编译器
# 内置变量：PROJECT_NAME=log；PROJECT_VERSION=1.0.0
project(log VERSION 1.0.0 LANGUAGES CXX)

# 全局C++标准：C++11
set(CMAKE_CXX_STANDARD 11)
# 强制必须使用该标准，编译器不支持就报错，不自动降级
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 关闭编译器GNU扩展，使用标准C++，提升跨平台兼容性（禁止gnu++11）
set(CMAKE_CXX_EXTENSIONS OFF)

# RUNTIME：可执行文件(.exe)输出到构建目录build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# ARCHIVE：静态库(.a/.lib)输出到build/
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# LIBRARY：动态库(.dll/.so)输出到build/
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

#------------------------------Threads线程库重点--------------------------------
# 查找系统线程库；REQUIRED代表找不到直接报错终止构建
# Windows(MinGW)：找到win32线程；Linux：找到pthread
find_package(Threads REQUIRED)

# 生成可执行目标 log，源文件 src/main.cpp
add_executable(log
    src/main.cpp
)

# 给目标log添加头文件搜索路径，PRIVATE仅本目标编译生效，不传递给其他链接方
# CMAKE_CURRENT_SOURCE_DIR：当前CMakeLists.txt所在源码根目录
target_include_directories(log
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/inc/include"
)

# 链接线程库：Threads::Threads 是CMake提供的**抽象导入目标**
# 不用关心底层是pthread还是win32 thread，CMake自动适配平台
target_link_libraries(log PRIVATE Threads::Threads)

# 解决 MinGW "too many sections" 问题
target_compile_options(log PRIVATE -O1 -fno-inline -flto)
target_link_options(log PRIVATE -flto=8)
# 给log目标单独指定编译特性C++11；
# 这里属于重复设置，前面已经全局 CMAKE_CXX_STANDARD 11；
# 好处：就算全局标准被覆盖，该目标依然强制C++11
target_compile_features(log PRIVATE cxx_std_11)
