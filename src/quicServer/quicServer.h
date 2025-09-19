#pragma once
#include <msquic.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>

class quicServer
{
public:
    quicServer(const std::string &certPath, const std::string &keyPath, boost::asio::io_context &io);
    ~quicServer();

    bool start(uint16_t port);
    void stop();
    bool sendMessage(HQUIC stream, const std::string &msg);

    // Callbacks
    std::function<void(HQUIC, HQUIC)> onClientConnected;
    std::function<void(HQUIC)> onClientDisconnected;
    std::function<void(HQUIC, const std::string &)> onMessageReceived;
    std::function<void(HQUIC, HQUIC)> onStreamStarted;

private:
    std::string certFile_;
    std::string keyFile_;
    boost::asio::io_context &io_;

    std::mutex clients_mutex_;
    struct ClientCtx
    {
        HQUIC Connection = nullptr;
        HQUIC Stream = nullptr;
    };
    std::unordered_map<HQUIC, ClientCtx> clients_;

    std::mutex recv_buffers_mutex_;
    std::unordered_map<HQUIC, std::string> recv_buffers_;

    std::string &recvBufferForStream(HQUIC stream);
    void handleSendComplete(void *client_context);

    // MsQuic handles
    const QUIC_API_TABLE *MsQuic = nullptr;
    HQUIC Registration = nullptr;
    HQUIC Configuration = nullptr;
    HQUIC Listener = nullptr;

    static QUIC_STATUS QUIC_API listenerCallback(HQUIC, void *, QUIC_LISTENER_EVENT *);
    static QUIC_STATUS QUIC_API connectionCallback(HQUIC, void *, QUIC_CONNECTION_EVENT *);
    static QUIC_STATUS QUIC_API streamCallback(HQUIC, void *, QUIC_STREAM_EVENT *);
};
