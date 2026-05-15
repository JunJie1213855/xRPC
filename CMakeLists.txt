#设置最低版本和项目名称
# 3.20 已支持 C++20 标准与现代 generator expressions；
# 不需要 CMake 4.x，因此把下限调低以匹配 Ubuntu 24.04 自带 3.28+ / GH Actions runner 上的 cmake。
cmake_minimum_required(VERSION 3.20)
project(Xrpc CXX)

#设置全局的C++标准（asio 协程需要 C++20）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)#CMAKE_CXX_STANDARD_REQUIRED表示强制使用指定的标准

# ZooKeeper多线程版本需要THREADED定义
add_definitions(-DTHREADED)

# 使用 standalone asio（非 Boost.Asio）
add_definitions(-DASIO_STANDALONE)

#设置头文件目录，供所有子项目使用
#include_directories(${CMAKE_SOURCE_DIR}/src/include) #其中CMAKE_SOURCE_DIR表示当前目录

#查找porotbuf包
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})#Protobuf_INCLUDE_DIRS表示protobuf头文件目录

#设置全局链接库（已从 muduo 迁移至 standalone asio header-only，不再链接 muduo）
set(LIBS
    protobuf
    pthread
    zookeeper_mt
    glog
)
# 启用 ctest（必须在顶层 CMakeLists.txt，否则子目录里 add_test 不会被 ctest 发现）
enable_testing()

#添加子目录
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)
