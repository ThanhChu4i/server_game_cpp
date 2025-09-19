#include "quicServer.h"
#include <iostream>
#include <cstring>
#include <stdexcept>

quicServer::quicServer(const std::string &certFile, const std::string &keyFile, boost::asio::io_context &io)
    : CertFile(certFile), KeyFile(keyFile), io_context_(io)
{
    if (QUIC_FAILED(MsQuicOpen2(&MsQuic)))
        throw std::runtime_error("MsQuicOpen2 failed");

    QUIC_REGISTRATION_CONFIG RegConfig{"GameServer", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration)))
        throw std::runtime_error("RegistrationOpen failed");
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
    // ALPN
    QUIC_BUFFER Alpn{};
    const char *AlpnStr = "game";
    Alpn.Buffer = (uint8_t *)AlpnStr;
    Alpn.Length = (uint32_t)strlen(AlpnStr);

    // Cert
    QUIC_CREDENTIAL_CONFIG CredConfig{};
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    QUIC_CERTIFICATE_FILE Cert{};
    Cert.CertificateFile = CertFile.c_str();
    Cert.PrivateKeyFile = KeyFile.c_str();
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

    std::cout << "QUIC Server running on port " << port << "\n";
    return true;
}

void quicServer::stop()
{
    for (auto &[id, c] : Clients)
    {
        if (c.Stream)
            MsQuic->StreamClose(c.Stream);
        if (c.Connection)
            MsQuic->ConnectionClose(c.Connection);
    }
    Clients.clear();

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

bool quicServer::sendMessage(uint64_t connId, const std::string &msg)
{
    auto it = Clients.find(connId);
    if (it == Clients.end() || !it->second.Stream)
        return false;

    QUIC_BUFFER buf;
    buf.Buffer = (uint8_t *)msg.data();
    buf.Length = (uint32_t)msg.size();

    return QUIC_SUCCEEDED(MsQuic->StreamSend(it->second.Stream, &buf, 1, QUIC_SEND_FLAG_NONE, nullptr));
}

// ================= Callbacks =================

QUIC_STATUS QUIC_API quicServer::listenerCallback(HQUIC, void *ctx, QUIC_LISTENER_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);
    if (evt->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION)
    {
        uint64_t id = self->NextConnId++;
        self->Clients[id] = {evt->NEW_CONNECTION.Connection, nullptr};

        self->MsQuic->SetCallbackHandler(evt->NEW_CONNECTION.Connection, (void *)connectionCallback, self);
        self->MsQuic->ConnectionSetConfiguration(evt->NEW_CONNECTION.Connection, self->Configuration);

        if (self->OnClientConnected)
            boost::asio::post(self->io_context_, [cb = self->OnClientConnected, id]()
                              { cb(id); });
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API quicServer::connectionCallback(HQUIC conn, void *ctx, QUIC_CONNECTION_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);
    uint64_t id = 0;
    for (auto &[cid, c] : self->Clients)
        if (c.Connection == conn)
        {
            id = cid;
            break;
        }

    switch (evt->Type)
    {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        std::cout << "Client " << id << " connected\n";
        break;
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        self->Clients[id].Stream = evt->PEER_STREAM_STARTED.Stream;
        self->MsQuic->SetCallbackHandler(evt->PEER_STREAM_STARTED.Stream, (void *)streamCallback, self);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        if (self->OnClientDisconnected)
            boost::asio::post(self->io_context_, [cb = self->OnClientDisconnected, id]()
                              { cb(id); });
        self->Clients.erase(id);
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API quicServer::streamCallback(HQUIC stream, void *ctx, QUIC_STREAM_EVENT *evt)
{
    auto *self = static_cast<quicServer *>(ctx);
    uint64_t id = 0;
    for (auto &[cid, c] : self->Clients)
        if (c.Stream == stream)
        {
            id = cid;
            break;
        }

    switch (evt->Type)
    {
    case QUIC_STREAM_EVENT_RECEIVE:
    {
        std::string msg((char *)evt->RECEIVE.Buffers[0].Buffer, evt->RECEIVE.Buffers[0].Length);
        if (self->OnMessageReceived)
            boost::asio::post(self->io_context_, [cb = self->OnMessageReceived, id, msg]()
                              { cb(id, msg); });
        break;
    }
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}
