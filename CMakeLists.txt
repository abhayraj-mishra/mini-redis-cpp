cmake_minimum_required(VERSION 3.10)
project(mini_redis_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(include)

add_executable(mini_redis
    src/main.cpp
    src/store.cpp
    src/server.cpp
)

target_link_libraries(mini_redis PRIVATE Threads::Threads)
