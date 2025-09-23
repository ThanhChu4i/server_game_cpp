#include "quicServer/quicServer.h"
#include "core/gameplay.h"
#include "init/init.h"
#include "boost/asio.hpp"
#include <curl/curl.h>
#include <iostream>
#include <memory>

using namespace boost::asio;
using namespace boost::asio::ip;

/// @brief Chạy server game
boost::asio::awaitable<void> runGameServer(io_context &io)
{
    std::cout << "Starting server..." << std::endl;

    // 1️⃣ Khởi tạo libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 2️⃣ Khởi tạo cơ sở dữ liệu
    init dbInitializer;
    if (!dbInitializer.Load())
    {
        std::cerr << "Failed to initialize Postgres or Redis." << std::endl;
        curl_global_cleanup();
        co_return;
    }

    // 3️⃣ Tạo server và gameplay
    auto server = std::make_unique<quicServer>("../certs/server.crt", "../certs/server.key", io);
    auto gameLogic = std::make_unique<Gameplay>(*server, io);

    // 4️⃣ Gắn callbacks, post vào io_context để thread-safe
    server->onMessageReceived = [gameLogic_ptr = gameLogic.get(), &io](HQUIC stream, const std::string &msg)
    {
        // log message raw nhận được
        std::cout << "[Server] Received raw msg: " << msg << std::endl;

        // đẩy sang io_context để xử lý
        boost::asio::post(io, [gameLogic_ptr, stream, msg]()
                          {
        if (gameLogic_ptr) 
            gameLogic_ptr->handleMessage(stream, msg); });
    };

    server->onStreamStarted = [gameLogic_ptr = gameLogic.get(), &io](HQUIC conn, HQUIC stream)
    {
        boost::asio::post(io, [gameLogic_ptr, conn, stream]()
                          {
            if (gameLogic_ptr) gameLogic_ptr->handlePlayerConnected(conn, stream); });
    };

    server->onClientDisconnected = [gameLogic_ptr = gameLogic.get(), &io](HQUIC stream)
    {
        boost::asio::post(io, [gameLogic_ptr, stream]()
                          {
            if (gameLogic_ptr) gameLogic_ptr->handlePlayerDisconnected(stream); });
    };

    // 5️⃣ Bắt đầu server
    if (!server->start(4443))
    {
        std::cerr << "Server start failed." << std::endl;
        curl_global_cleanup();
        co_return;
    }

    gameLogic->startGameLoop();
    std::cout << "Server is running. Press Enter to stop..." << std::endl;

    // 6️⃣ Signal handler Ctrl+C
    signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto)
                       {
        std::cout << "Signal received. Stopping server..." << std::endl;
        gameLogic->stopGameLoop();
        server->stop();
        curl_global_cleanup();
        io.stop(); });

    // 7️⃣ Chờ Enter
    posix::stream_descriptor input(io, ::dup(STDIN_FILENO));
    std::string buffer;
    co_await async_read_until(input, dynamic_buffer(buffer), '\n', use_awaitable);

    // 8️⃣ Dừng server khi nhấn Enter
    std::cout << "Stopping server..." << std::endl;
    gameLogic->stopGameLoop();
    server->stop();
    curl_global_cleanup();

    std::cout << "Server stopped. All resources cleaned up. :)" << std::endl;
}

// ---
int main()
{
    io_context io;

    // Spawn coroutine runGameServer
    co_spawn(io, runGameServer(io), detached);

    io.run(); // Chạy event loop
    return 0;
}
