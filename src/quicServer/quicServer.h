#ifndef QUIC_SERVER_H
#define QUIC_SERVER_H

#include <msquic.h>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>

// Định nghĩa HQUIC dưới dạng một kiểu dữ liệu có thể dễ dàng sử dụng
using HQUIC = QUIC_HANDLE *;

class Gameplay;
class quicServer
{
public:
    // Cấu trúc để lưu trữ thông tin client
    struct Client
    {
        HQUIC Connection;
        HQUIC Stream;
    };

    quicServer(const std::string &certPath, const std::string &keyPath, boost::asio::io_context &io);
    ~quicServer();
    // Khởi động server trên một cổng cụ thể
    bool start(uint16_t port);

    // Dừng server và dọn dẹp tài nguyên
    void stop();

    // Gửi tin nhắn đến một stream cụ thể
    bool sendMessage(HQUIC stream, const std::string &msg);

    // Các callbacks để xử lý sự kiện
    std::function<void(HQUIC, HQUIC)> onStreamStarted;
    std::function<void(HQUIC)> onClientDisconnected;
    std::function<void(HQUIC, const std::string &)> onMessageReceived;
    std::function<void(HQUIC conn, HQUIC stream)> onClientConnected;

private:
    const std::string certFile_;
    const std::string keyFile_;
    boost::asio::io_context &io_;

    // MsQuic và các đối tượng liên quan
    const QUIC_API_TABLE *MsQuic;
    HQUIC Registration;
    HQUIC Configuration;
    HQUIC Listener;

    // Quản lý các clients đã kết nối
    std::map<HQUIC, Client> clients_;
    std::mutex clients_mutex_;

    // Buffer để xử lý dữ liệu nhận được
    std::map<HQUIC, std::string> recv_buffers_;
    std::mutex recv_buffers_mutex_;
    Gameplay *gameplay_ = nullptr;
    // Hàm callback tĩnh được gọi bởi MsQuic
    static QUIC_STATUS QUIC_API listenerCallback(HQUIC Listener, void *ctx, QUIC_LISTENER_EVENT *evt);
    static QUIC_STATUS QUIC_API connectionCallback(HQUIC Connection, void *ctx, QUIC_CONNECTION_EVENT *evt);
    static QUIC_STATUS QUIC_API streamCallback(HQUIC Stream, void *ctx, QUIC_STREAM_EVENT *evt);

    // Hàm hỗ trợ
    void handleSendComplete(void *client_context);
    std::string &recvBufferForStream(HQUIC stream);
};

#endif // QUIC_SERVER_H