cmake_minimum_required(VERSION 3.16)
project(distributed-kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

find_package(Threads REQUIRED)

add_library(kvstore_core STATIC
    src/common/protocol.cpp
    src/common/logger.cpp
    src/storage/kv_store.cpp
    src/storage/wal.cpp
    src/network/connection.cpp
    src/network/tcp_server.cpp
    src/network/tcp_client.cpp
    src/threading/thread_pool.cpp
    src/replication/replication_manager.cpp
    src/server/config.cpp
    src/server/node.cpp
    src/client/kv_client.cpp
)
target_include_directories(kvstore_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kvstore_core PUBLIC Threads::Threads)

add_executable(kvstore-server src/main.cpp)
target_link_libraries(kvstore-server PRIVATE kvstore_core)

add_executable(kvstore-bench
    src/bench/bench_main.cpp
    src/bench/load_generator.cpp
    src/bench/stats.cpp
)
target_include_directories(kvstore-bench PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kvstore-bench PRIVATE kvstore_core)
