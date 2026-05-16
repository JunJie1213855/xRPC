#设置最低版本和项目名称
# 3.20 已支持 C++20 标准与现代 generator expressions；
# 不需要 CMake 4.x，因此把下限调低以匹配 Ubuntu 24.04 自带 3.28+ / GH Actions runner 上的 cmake。
cmake_minimum_required(VERSION 3.20)
project(Xrpc CXX)

# 排除 miniconda3，防止其 fmt/spdlog 与系统 apt 版本冲突
list(APPEND CMAKE_IGNORE_PATH /home/ros/miniconda3)

#设置全局的C++标准（asio 协程需要 C++20）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)#CMAKE_CXX_STANDARD_REQUIRED表示强制使用指定的标准

# ZooKeeper多线程版本需要THREADED定义
add_definitions(-DTHREADED)

# 使用 standalone asio（非 Boost.Asio）
add_definitions(-DASIO_STANDALONE)

#设置头文件目录，供所有子项目使用
#include_directories(${CMAKE_SOURCE_DIR}/src/include) #其中CMAKE_SOURCE_DIR表示当前目录

# /usr/local 安装的 protobuf 6.x 依赖 abseil；
# 用 pkg-config 获取完整的库列表（含所有 absl_* 传递依赖），
# 绕开 cmake imported target 的 NOCONFIG/Release 配置映射问题。
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)

# 保留 find_package(Protobuf) 以获取头文件目录和 protoc 路径
list(APPEND CMAKE_PREFIX_PATH /usr/local)
find_package(Protobuf QUIET)
if(Protobuf_FOUND)
    include_directories(${Protobuf_INCLUDE_DIRS})
else()
    include_directories(${PROTOBUF_INCLUDE_DIRS})
endif()

#设置全局链接库（已从 muduo 迁移至 standalone asio header-only，不再链接 muduo）
find_package(spdlog REQUIRED)

set(LIBS
    PkgConfig::PROTOBUF
    pthread
    zookeeper_mt
    spdlog::spdlog
)
# 启用 ctest（必须在顶层 CMakeLists.txt，否则子目录里 add_test 不会被 ctest 发现）
enable_testing()

#添加子目录
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)
