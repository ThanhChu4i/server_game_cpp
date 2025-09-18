#include "AsioService.h"
#include <iostream>

AsioService::AsioService()
    : ioContext_(std::make_unique<boost::asio::io_context>())
{
    // Work guard giữ io_context chạy liên tục
    workGuard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(ioContext_->get_executor());
}

AsioService::~AsioService()
{
    stop(); // đảm bảo dừng thread khi object bị destroy
}

boost::asio::io_context &AsioService::getContext()
{
    return *ioContext_;
}

void AsioService::start()
{
    asioThread_ = std::thread([this]()
                              { ioContext_->run(); });
}

void AsioService::stop()
{
    if (ioContext_)
    {
        ioContext_->stop(); // dừng vòng lặp
    }
    if (asioThread_.joinable())
    {
        asioThread_.join(); // chờ thread kết thúc
    }
    // Hủy work guard để giải phóng resource
    if (workGuard_)
    {
        workGuard_.reset();
    }
}
