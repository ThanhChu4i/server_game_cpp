cmake_minimum_required(VERSION 3.15)
project(GameQuicServer CXX)

#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Bật coroutines cho GCC/Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()
# Đường dẫn msquic
set(MSQUIC_DIR ${CMAKE_SOURCE_DIR}/extern/msquic)
include_directories(${MSQUIC_DIR}/src/inc)
link_directories(${MSQUIC_DIR}/build/bin/Release)

# Redis++ trong extern
set(REDIS_PLUS_PLUS_DIR ${CMAKE_SOURCE_DIR}/extern/redis-plus-plus/build/install)
include_directories(${REDIS_PLUS_PLUS_DIR}/include)
link_directories(${REDIS_PLUS_PLUS_DIR}/lib)

add_executable(server
    src/server.cpp
    src/QuicServer/QuicServer.cpp
    src/AsioService/AsioService.cpp
    src/Database/Postgres/PostgresClient.cpp
    src/Database/Redis/RedisClient.cpp
    src/Init/Init.cpp
)

# Copy config
file(COPY ${CMAKE_SOURCE_DIR}/src/Database/Postgres/config.json
     DESTINATION ${CMAKE_BINARY_DIR}/Database/Postgres)
file(COPY ${CMAKE_SOURCE_DIR}/src/Database/Redis/config.json
     DESTINATION ${CMAKE_BINARY_DIR}/Database/Redis)

# System Hiredis
find_path(HIREDIS_INCLUDE hiredis/hiredis.h)

find_library(HIREDIS_LIB hiredis)

# PostgreSQL
find_package(PostgreSQL REQUIRED)

# nlohmann/json
find_package(nlohmann_json 3.2.0 REQUIRED)

# curl/curl.j
find_package(CURL REQUIRED)

#boot.asio
find_package(Boost REQUIRED COMPONENTS system thread)

target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${HIREDIS_INCLUDE}
    ${PostgreSQL_INCLUDE_DIRS}
)

# Include Redis++
include_directories(${CMAKE_SOURCE_DIR}/extern/redis-plus-plus/build/install/include)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src/Coroutine) #Task.h
link_directories(${CMAKE_SOURCE_DIR}/extern/redis-plus-plus/build/install/lib)

target_link_libraries(server
    PRIVATE
    ${HIREDIS_LIB}                     # system Hiredis
    ${PostgreSQL_LIBRARIES}            # PostgreSQL
    ${CURL_LIBRARIES}                  # curl/curl.h
    ${Boost_LIBRARIES}                 # boot.asio
    nlohmann_json::nlohmann_json
    msquic
    ssl
    ${CMAKE_SOURCE_DIR}/extern/redis-plus-plus/build/libredis++.a                         # link trực tiếp tới libredis++.a hoặc .so
    crypto
    pthread
    dl
)