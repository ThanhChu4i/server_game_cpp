#include "quicServer/quicServer.h"
#include "database/postgres/postgresClient.h"
#include "database/redis/redisClient.h"
#include "message/clientContext.h"
#include "message/messageDispatcher.h"
#include "message/messageHandler.h"
#include "AsioService/AsioService.h"
#include "init/init.h"
#include "boost/asio.hpp"
#include <curl/curl.h>
#include <iostream>

using namespace boost::asio;

boost::asio::awaitable<void> runServer(io_context &io)
{
    AsioService asioService; // chứa io_context bên trong
    MessageDispatcher dispatcher;

    curl_global_init(CURL_GLOBAL_DEFAULT); // khởi tạo curl

    // Khởi tạo Postgres + Redis
    init init_;
    if (!init_.Load())
    {
        std::cerr << "Failed to initialize Postgres or Redis" << std::endl;
        co_return;
    }

    // Khởi tạo QUIC server, truyền io_context
    quicServer server("../certs/server.crt", "../certs/server.key", io);
    if (!server.start(4443))
    {
        std::cerr << "Server start failed\n";
        co_return;
    }

    asioService.start(); // start asio loop

    std::cout << "Server running. Press Enter to stop...\n";

    // Đọc stdin async
    posix::stream_descriptor input(io, ::dup(STDIN_FILENO));
    std::string buffer;
    co_await async_read(input, dynamic_buffer(buffer), use_awaitable);

    std::cout << "Stop Server...\n";
    server.stop();

    std::cout << "Stop curl\n";
    curl_global_cleanup();

    std::cout << "Stop AsioService...\n";
    asioService.stop();

    std::cout << "Done... :)\n";
}

int main()
{
    io_context io;

    // Spawn coroutine runServer
    co_spawn(io, runServer(io), detached);

    io.run(); // chạy event loop
    return 0;
}
