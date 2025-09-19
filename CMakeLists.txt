cmake_minimum_required(VERSION 3.15)
project(GameQuicServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bật coroutines cho GCC/Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fcoroutines)
endif()

# Tìm kiếm các thư viện ngoài một cách hiện đại
find_package(Boost 1.70 REQUIRED COMPONENTS system thread) # Yêu cầu Boost 1.70 trở lên
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)

# Đường dẫn msquic
set(MSQUIC_DIR ${CMAKE_SOURCE_DIR}/include/msquic)
include_directories(${MSQUIC_DIR}/src/inc)
link_directories(${MSQUIC_DIR}/build/bin/Release)

# Thay vì tìm hiredis hệ thống, bạn đã có redis-plus-plus.
# System Hiredis
find_path(HIREDIS_INCLUDE hiredis/hiredis.h)
find_library(HIREDIS_LIB hiredis)
# ...
# Redis++ đã bao gồm hiredis, nên chỉ cần liên kết với redis++
set(REDIS_PLUS_PLUS_DIR ${CMAKE_SOURCE_DIR}/include/redis-plus-plus/build/install)
set(REDIS_PLUS_PLUS_LIB ${CMAKE_SOURCE_DIR}/include/redis-plus-plus/build/libredis++.a)

# Cấu hình MSQuic
set(MSQUIC_DIR ${CMAKE_SOURCE_DIR}/include/msquic)
# Thêm đường dẫn include và link một cách an toàn hơn
add_library(msquic_lib STATIC IMPORTED)
set_target_properties(msquic_lib PROPERTIES 
    IMPORTED_LOCATION "${MSQUIC_DIR}/build/bin/Release/msquic.lib" # hoặc .a trên Linux
    INTERFACE_INCLUDE_DIRECTORIES "${MSQUIC_DIR}/src/inc"
)

add_executable(server
    src/server.cpp
    src/quicServer/quicServer.cpp
    src/AsioService/AsioService.cpp
    src/database/postgres/postgresClient.cpp
    src/database/redis/redisClient.cpp
    src/init/init.cpp
    src/core/gameplay.cpp
)

# Copy config
file(COPY ${CMAKE_SOURCE_DIR}/src/database/postgres/config.json
     DESTINATION ${CMAKE_BINARY_DIR}/database/postgres)
file(COPY ${CMAKE_SOURCE_DIR}/src/database/redis/config.json
     DESTINATION ${CMAKE_BINARY_DIR}/database/redis)

# Tùy chỉnh target include directories
# PostgreSQL
find_package(PostgreSQL REQUIRED)
target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${REDIS_PLUS_PLUS_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/Coroutine #Task.h
    ${HIREDIS_INCLUDE}
    ${PostgreSQL_INCLUDE_DIRS}
)

include_directories(${MSQUIC_DIR}/src/inc)
# Liên kết các thư viện một cách an toàn
target_link_libraries(server PRIVATE
    ${HIREDIS_LIB}                     # system Hiredis <-- Đã có nhưng có thể sai vị trí
    ${CMAKE_SOURCE_DIR}/include/redis-plus-plus/build/libredis++.a # libredis++.a
    ${Boost_LIBRARIES}                 # Hoặc Boost::system Boost::thread
    nlohmann_json::nlohmann_json
    ${PostgreSQL_LIBRARIES}            # PostgreSQL
    ${CURL_LIBRARIES}                  # curl/curl.h
    ${REDIS_PLUS_PLUS_LIB}
    msquic
    ssl
    crypto
    pthread
    dl
)
