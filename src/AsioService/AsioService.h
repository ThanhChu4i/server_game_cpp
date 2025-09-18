#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <thread>

class AsioService
{
public:
    AsioService();
    ~AsioService();

    // Lấy reference đến io_context
    boost::asio::io_context &getContext();

    // Bắt đầu loop trong thread riêng
    void start();

    // Dừng loop và join thread
    void stop();

private:
    std::unique_ptr<boost::asio::io_context> ioContext_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard_;
    std::thread asioThread_;
};
