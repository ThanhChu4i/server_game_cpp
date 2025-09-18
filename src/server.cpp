#include "QuicServer/QuicServer.h"
#include "Database/Postgres/PostgresClient.h"
#include "Database/Redis/RedisClient.h"
#include "Message/ClientContext.h"
#include "Message/MessageDispatcher.h"
#include "Message/MessageHandler.h"
#include "AsioService/AsioService.h"
#include "Init/Init.h"
#include "boost/asio.hpp"
#include <curl/curl.h>
#include <iostream>

int main()
{
    AsioService asioService;
    MessageDispatcher dispatcher;          // Bộ phân phối xử lý message giữa client và server
    curl_global_init(CURL_GLOBAL_DEFAULT); // khoi tao curl
    // Khởi tạo kết nối tới Postgres với thông tin đăng nhập
    Init init; // Tạo instance Init
    if (!init.Load())
    { // Gọi method non-static
        std::cerr << "Failed to initialize Postgres or Redis" << std::endl;
        return 1;
    }
    // Khởi tạo QUIC server với đường dẫn tới file chứng chỉ và private key
    QuicServer server("../certs/server.crt", "../certs/server.key");
    // Khởi động server trên port 4443
    if (!server.Start(4443))
    {
        std::cerr << "Server start failed\n";
        return 1;
    }
    // Run Asio loop
    asioService.start();
    std::cout
        << "Press Enter to stop...\n";
    std::cin.get(); // Chờ người dùng nhấn Enter để dừng server
    std::cout << "Stop Server...\n";
    server.Stop(); // Dừng server và giải phóng tài nguyên
    std::cout << "Stop curl\n";
    curl_global_cleanup();
    std::cout << "Stop AsioService...\n";
    asioService.stop(); // dừng loop
    std::cout << "Done... :)\n";
    return 0;
}