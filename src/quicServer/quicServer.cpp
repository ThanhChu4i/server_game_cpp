#include "quicServer.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "../core/gameplay.h"

static std::map<HQUIC, HQUIC> g_StreamToConnection;
static std::mutex g_PlayersMutex;
static std::mutex g_StreamToConnectionMutex;
static std::map<HQUIC, Player> g_Players;
quicServer::quicServer(const std::string &certPath, const std::string &keyPath, boost::asio::io_context &io)
    : certFile_(certPath), keyFile_(keyPath), io_(io), MsQuic(nullptr), Registration(nullptr), Configuration(nullptr), Listener(nullptr)
{
    if (MsQuicOpen2(&MsQuic) != QUIC_STATUS_SUCCESS)
        throw std::runtime_error("Failed to open MsQuic");

    QUIC_REGISTRATION_CONFIG regConfig{"quicServer", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (MsQuic->RegistrationOpen(&regConfig, &Registration) != QUIC_STATUS_SUCCESS)
        throw std::runtime_error("Failed to open MsQuic Registration");
}

quicServer::~quicServer()
{
    stop();

    if (Registration)
        MsQuic->RegistrationClose(Registration);
    if (MsQuic)
        MsQuicClose(MsQuic);
}

bool quicServer::start(uint16_t port)
{
    // Cấu hình ALPN (Application-Layer Protocol Negotiation)
    QUIC_BUFFER Alpn{};
    const char *AlpnStr = "game"; // Đảm bảo khớp với client
    Alpn.Buffer = (uint8_t *)AlpnStr;
    Alpn.Length = (uint32_t)strlen(AlpnStr);

    // Cấu hình chứng chỉ SSL/TLS
    QUIC_CREDENTIAL_CONFIG CredConfig{};
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    QUIC_CERTIFICATE_FILE Cert{};
    Cert.CertificateFile = certFile_.c_str();
    Cert.PrivateKeyFile = keyFile_.c_str();
    CredConfig.CertificateFile = &Cert;

    // --- Mở cấu hình QUIC trước ---
    if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &Alpn, 1, nullptr, 0, nullptr, &Configuration)))
        return false;

    // --- Set QUIC settings sau khi có Configuration ---
    QUIC_SETTINGS settings{};
    settings.IsSet.PeerBidiStreamCount = TRUE;
    settings.PeerBidiStreamCount = 10000;

    if (QUIC_FAILED(MsQuic->ConfigurationOpen(
            Registration,
            &Alpn, 1,
            &settings, sizeof(settings),
            nullptr,
            &Configuration)))
        return false;

    // Tải chứng chỉ
    if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig)))
        return false;

    // Mở listener
    if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, listenerCallback, this, &Listener)))
        return false;

    QUIC_ADDR addr{};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&addr, port);

    // Bắt đầu lắng nghe
    if (QUIC_FAILED(MsQuic->ListenerStart(Listener, &Alpn, 1, &addr)))
        return false;

    std::cout << "[QUIC] Server listening on port " << port << "\n";
    return true;
}

void quicServer::stop()
{
    // Đóng tất cả connections và streams
    {
        std::lock_guard<std::mutex> g(clients_mutex_);
        for (auto &kv : clients_)
        {
            if (kv.second.Stream)
                MsQuic->StreamClose(kv.second.Stream);
            if (kv.second.Connection)
                MsQuic->ConnectionClose(kv.second.Connection);
        }
        clients_.clear();
    }

    // Đóng listener và cấu hình
    if (Listener)
    {
        MsQuic->ListenerClose(Listener);
        Listener = nullptr;
    }
    if (Configuration)
    {
        MsQuic->ConfigurationClose(Configuration);
        Configuration = nullptr;
    }
}

bool quicServer::sendMessage(HQUIC stream, const std::string &msg)
{
    if (!stream)
        return false;

    // Gói chung string + buffer vào 1 struct
    struct SendContext
    {
        std::string data;
        QUIC_BUFFER buf;
    };

    auto *ctx = new SendContext{msg, {}};
    ctx->buf.Buffer = (uint8_t *)ctx->data.data();
    ctx->buf.Length = static_cast<uint32_t>(ctx->data.size());

    QUIC_STATUS status = MsQuic->StreamSend(
        stream,
        &ctx->buf,
        1,
        QUIC_SEND_FLAG_NONE,
        ctx // context giữ luôn cả string + buffer
    );

    if (QUIC_FAILED(status))
    {
        delete ctx;
        std::cerr << "[QUIC] StreamSend failed: 0x"
                  << std::hex << status << std::dec << "\n";
        return false;
    }

    return true;
}

std::string &quicServer::recvBufferForStream(HQUIC stream)
{
    std::lock_guard<std::mutex> lk(recv_buffers_mutex_);
    // tạo buffer nếu chưa tồn tại
    return recv_buffers_[stream];
}
void quicServer::handleSendComplete(void *client_context)
{
    if (!client_context)
        return;
    delete reinterpret_cast<std::string *>(client_context);
}

// ---------- static callbacks ----------
QUIC_STATUS QUIC_API quicServer::listenerCallback(HQUIC, void *ctx, QUIC_LISTENER_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);
    if (evt->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION)
    {
        HQUIC conn = evt->NEW_CONNECTION.Connection;
        {
            std::lock_guard<std::mutex> lk(self->clients_mutex_);
            self->clients_[conn] = {conn, nullptr};
        }
        self->MsQuic->SetCallbackHandler(conn, (void *)connectionCallback, self);
        self->MsQuic->ConnectionSetConfiguration(conn, self->Configuration);
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API quicServer::connectionCallback(HQUIC conn, void *ctx, QUIC_CONNECTION_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);
    std::cout << evt->Type << "\n";
    switch (evt->Type)
    {
    case QUIC_CONNECTION_EVENT_CONNECTED:
    {
        std::cout << "[QUIC] Client connected\n";
        // KHÔNG mở stream chủ động từ server
        break;
    }
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
    {
        std::cout << "[QUIC] PEER_STREAM_STARTED\n";
        HQUIC stream = evt->PEER_STREAM_STARTED.Stream;
        {
            std::lock_guard<std::mutex> lk(self->clients_mutex_);
            auto it = self->clients_.find(conn);
            if (it != self->clients_.end())
            {
                it->second.Stream = stream;
            }
        }
        self->MsQuic->SetCallbackHandler(stream, (void *)streamCallback, self);
        self->MsQuic->StreamReceiveSetEnabled(stream, TRUE);

        if (self->onStreamStarted)
        {
            boost::asio::post(self->io_, [cb = self->onStreamStarted, conn, stream]()
                              { cb(conn, stream); });
        }
        break;
    }
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
    {
        HQUIC stream_to_remove = nullptr;
        {
            std::lock_guard<std::mutex> lk(self->clients_mutex_);
            std::cout << "QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE\n";
            auto it = self->clients_.find(conn);
            if (it != self->clients_.end())
            {
                stream_to_remove = it->second.Stream;
                self->clients_.erase(it);
            }
        }
        if (stream_to_remove)
        {
            boost::asio::post(self->io_, [self, stream_to_remove]()
                              {
                if (self->onClientDisconnected) {
                    self->onClientDisconnected(stream_to_remove);
                } });
        }
        std::cout << "[QUIC] Connection shutdown\n";
        break;
    }
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API quicServer::streamCallback(HQUIC stream, void *ctx, QUIC_STREAM_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);

    switch (evt->Type)
    {
    case QUIC_STREAM_EVENT_RECEIVE:
    {
        for (uint32_t i = 0; i < evt->RECEIVE.BufferCount; ++i)
        {
            auto data = std::make_shared<std::string>(
                (char *)evt->RECEIVE.Buffers[i].Buffer,
                evt->RECEIVE.Buffers[i].Length);
            boost::asio::post(self->io_, [self, stream, data]()
                              {
            std::string &buf = self->recvBufferForStream(stream);
            buf.append(*data);
            size_t pos;
            while ((pos = buf.find('\n')) != std::string::npos) {
                std::string oneMsg = buf.substr(0, pos);
                buf.erase(0, pos + 1);
                if (!oneMsg.empty() && self->onMessageReceived) {
                    self->onMessageReceived(stream, oneMsg);
                }
            } });
        }
        break;
    }
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
    {
        // giải phóng dữ liệu send xong
        self->handleSendComplete(evt->SEND_COMPLETE.ClientContext);
        break;
    }
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        // Xử lý nếu cần
        break;
    default:
        break;
    }

    return QUIC_STATUS_SUCCESS;
}