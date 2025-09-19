#include "quicServer/quicServer.h"
#include "core/gameplay.h"
#include "init/init.h"
#include "boost/asio.hpp"
#include <curl/curl.h>
#include <iostream>
#include <memory>

using namespace boost::asio;
using namespace boost::asio::ip;

/// @brief Chứa logic chính để khởi tạo và chạy server game.
/// @param io Đối tượng io_context của Boost.Asio.
/// @return Một awaitable coroutine.
boost::asio::awaitable<void> runGameServer(io_context &io)
{
    std::cout << "Starting server..." << std::endl;

    // 1. Khởi tạo các thư viện toàn cục
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 2. Khởi tạo cơ sở dữ liệu
    init dbInitializer;
    if (!dbInitializer.Load())
    {
        std::cerr << "Failed to initialize Postgres or Redis." << std::endl;
        curl_global_cleanup(); // Dọn dẹp tài nguyên
        co_return;
    }

    // 3. Khởi tạo các đối tượng chính
    auto server = std::make_unique<quicServer>("../certs/server.crt", "../certs/server.key", io);
    auto gameLogic = std::make_unique<Gameplay>(*server);

    // 4. Gắn các callbacks của server
    server->onMessageReceived = [&gameLogic](HQUIC stream, const std::string &msg)
    {
        gameLogic->handleMessage(stream, msg);
    };

    server->onStreamStarted = [&gameLogic](HQUIC conn, HQUIC stream)
    {
        gameLogic->handlePlayerConnected(conn, stream);
    };

    server->onClientDisconnected = [&gameLogic](HQUIC stream)
    {
        gameLogic->handlePlayerDisconnected(stream);
    };

    // 5. Bắt đầu server và game loop
    if (!server->start(4443))
    {
        std::cerr << "Server start failed." << std::endl;
        curl_global_cleanup();
        co_return;
    }

    gameLogic->startGameLoop();

    std::cout << "Server is running. Press Enter to stop..." << std::endl;

    // 6. Chờ tín hiệu dừng từ người dùng
    posix::stream_descriptor input(io, ::dup(STDIN_FILENO));
    std::string buffer;
    co_await async_read(input, dynamic_buffer(buffer), use_awaitable);

    // 7. Dừng và dọn dẹp tài nguyên
    std::cout << "Stopping server..." << std::endl;
    gameLogic->stopGameLoop();
    server->stop();
    curl_global_cleanup();

    std::cout << "Server stopped. All resources cleaned up. :)" << std::endl;
}

//---
int main()
{
    io_context io;

    // Spawn coroutine runServer
    co_spawn(io, runGameServer(io), detached);

    io.run(); // chạy event loop
    return 0;
}
