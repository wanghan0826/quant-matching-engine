cmake_minimum_required(VERSION 3.10)
project(QuantEngine)

# 量化开发通常使用 C++17 或更新标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 O3 最高级别优化，多线程支持，以及严格的警告提示
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread")

# 指定可执行文件名为 ome_test，源码路径为 src/main.cpp
add_executable(ome_test src/main.cpp)