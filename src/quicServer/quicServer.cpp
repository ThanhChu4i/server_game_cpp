#include "quicServer.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>

quicServer::quicServer(const std::string &certPath, const std::string &keyPath, boost::asio::io_context &io)
    : certFile_(certPath), keyFile_(keyPath), io_(io)
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
    QUIC_BUFFER Alpn{};
    const char *AlpnStr = "game";
    Alpn.Buffer = (uint8_t *)AlpnStr;
    Alpn.Length = (uint32_t)strlen(AlpnStr);

    QUIC_CREDENTIAL_CONFIG CredConfig{};
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    QUIC_CERTIFICATE_FILE Cert{};
    Cert.CertificateFile = certFile_.c_str();
    Cert.PrivateKeyFile = keyFile_.c_str();
    CredConfig.CertificateFile = &Cert;

    if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &Alpn, 1, nullptr, 0, nullptr, &Configuration)))
        return false;

    if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig)))
        return false;

    if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, listenerCallback, this, &Listener)))
        return false;

    QUIC_ADDR addr{};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&addr, port);

    if (QUIC_FAILED(MsQuic->ListenerStart(Listener, &Alpn, 1, &addr)))
        return false;

    std::cout << "[QUIC] Server listening on port " << port << "\n";
    return true;
}

void quicServer::stop()
{
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

    auto *sendData = new std::string(msg);
    QUIC_BUFFER buf;
    buf.Buffer = (uint8_t *)sendData->data();
    buf.Length = (uint32_t)sendData->size();

    QUIC_STATUS status = MsQuic->StreamSend(stream, &buf, 1, QUIC_SEND_FLAG_NONE, sendData);
    if (QUIC_FAILED(status))
    {
        delete sendData;
        std::cerr << "[QUIC] StreamSend failed: 0x" << std::hex << status << std::dec << "\n";
        return false;
    }
    return true;
}

std::string &quicServer::recvBufferForStream(HQUIC stream)
{
    std::lock_guard<std::mutex> lk(recv_buffers_mutex_);
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

    switch (evt->Type)
    {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        std::cout << "[QUIC] Client connected\n";
        if (self->onClientConnected)
            boost::asio::post(self->io_, [cb = self->onClientConnected, conn]()
                              { cb(conn, conn); });
        break;

    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
    {
        std::lock_guard<std::mutex> lk(self->clients_mutex_);
        auto it = self->clients_.find(conn);
        if (it != self->clients_.end())
        {
            it->second.Stream = evt->PEER_STREAM_STARTED.Stream;
            self->MsQuic->SetCallbackHandler(it->second.Stream, (void *)streamCallback, self);
            self->MsQuic->StreamReceiveSetEnabled(it->second.Stream, TRUE);
            if (self->onStreamStarted)
                boost::asio::post(self->io_, [cb = self->onStreamStarted, conn, stream = it->second.Stream]()
                                  { cb(conn, stream); });
        }
        break;
    }

    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
    {
        std::vector<HQUIC> to_remove;
        std::lock_guard<std::mutex> lk(self->clients_mutex_);
        for (auto &kv : self->clients_)
        {
            if (kv.second.Connection == conn)
            {
                HQUIC stream = kv.second.Stream;
                if (self->onClientDisconnected)
                    boost::asio::post(self->io_, [cb = self->onClientDisconnected, stream]()
                                      { cb(stream); });
                to_remove.push_back(kv.first);
            }
        }
        for (auto k : to_remove)
            self->clients_.erase(k);
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
        std::string &buf = self->recvBufferForStream(stream);
        buf.append((char *)evt->RECEIVE.Buffers[0].Buffer, evt->RECEIVE.Buffers[0].Length);

        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos)
        {
            std::string oneMsg = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (!oneMsg.empty() && self->onMessageReceived)
                boost::asio::post(self->io_, [cb = self->onMessageReceived, stream, oneMsg]()
                                  { cb(stream, oneMsg); });
        }
        break;
    }

    case QUIC_STREAM_EVENT_SEND_COMPLETE:
        self->handleSendComplete(evt->SEND_COMPLETE.ClientContext);
        break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
    {
        std::lock_guard<std::mutex> lk(self->recv_buffers_mutex_);
        self->recv_buffers_.erase(stream);
    }
    break;

    default:
        break;
    }

    return QUIC_STATUS_SUCCESS;
}
