#pragma once
#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include "msquic.h"

struct Client
{
    HQUIC Connection{nullptr};
    HQUIC Stream{nullptr};
};

class quicServer
{
public:
    quicServer(const std::string &certFile, const std::string &keyFile, boost::asio::io_context &io);
    ~quicServer();

    bool start(uint16_t port);
    void stop();
    bool sendMessage(uint64_t connId, const std::string &msg);

    // Callbacks
    std::function<void(uint64_t)> OnClientConnected;
    std::function<void(uint64_t)> OnClientDisconnected;
    std::function<void(uint64_t, const std::string &)> OnMessageReceived;

private:
    std::string CertFile, KeyFile;
    boost::asio::io_context &io_context_;

    const QUIC_API_TABLE *MsQuic{nullptr}; // bảng API MsQuic
    HQUIC Registration{nullptr};
    HQUIC Listener{nullptr};
    HQUIC Configuration{nullptr};

    uint64_t NextConnId{1};
    std::unordered_map<uint64_t, Client> Clients;

    static QUIC_STATUS QUIC_API listenerCallback(HQUIC, void *ctx, QUIC_LISTENER_EVENT *evt);
    static QUIC_STATUS QUIC_API connectionCallback(HQUIC conn, void *ctx, QUIC_CONNECTION_EVENT *evt);
    static QUIC_STATUS QUIC_API streamCallback(HQUIC stream, void *ctx, QUIC_STREAM_EVENT *evt);
};
