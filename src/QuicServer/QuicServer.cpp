#include "QuicServer.h"
#include <iostream>
#include <cstring>

// ======================= Constructor =======================
// Khởi tạo server với đường dẫn đến chứng chỉ TLS (certFile) và private key (keyFile)
QuicServer::QuicServer(const std::string &certFile, const std::string &keyFile)
    : CertFile(certFile), KeyFile(keyFile)
{
    // Khởi tạo API MsQuic
    if (QUIC_FAILED(MsQuicOpen2(&MsQuic)))
    {
        throw std::runtime_error("MsQuicOpen2 failed");
    }

    // Đăng ký cấu hình cho ứng dụng QUIC
    QUIC_REGISTRATION_CONFIG RegConfig{"GameServer", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration)))
    {
        throw std::runtime_error("RegistrationOpen failed");
    }
}

// ======================= Destructor =======================
// Dọn dẹp tài nguyên khi đối tượng bị hủy
QuicServer::~QuicServer()
{
    Stop(); // Dừng server trước
    if (Registration)
        MsQuic->RegistrationClose(Registration);
    if (MsQuic)
        MsQuicClose(MsQuic);
}

// ======================= Start Server =======================
bool QuicServer::Start(uint16_t port)
{
    // Chuẩn bị thông tin ALPN
    QUIC_BUFFER Alpn;
    const char *AlpnStr = "game";
    Alpn.Buffer = (uint8_t *)AlpnStr;
    Alpn.Length = (uint32_t)strlen(AlpnStr);

    // Thiết lập chứng chỉ TLS
    QUIC_CREDENTIAL_CONFIG CredConfig{};
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    QUIC_CERTIFICATE_FILE Cert{};
    Cert.CertificateFile = CertFile.c_str();
    Cert.PrivateKeyFile = KeyFile.c_str();
    CredConfig.CertificateFile = &Cert;
    CredConfig.Flags = QUIC_CREDENTIAL_FLAG_NONE;

    // Mở cấu hình QUIC
    if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &Alpn, 1, nullptr, 0, nullptr, &Configuration)))
    {
        std::cerr << "ConfigurationOpen failed\n";
        return false;
    }

    // Load chứng chỉ TLS
    if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig)))
    {
        std::cerr << "ConfigurationLoadCredential failed\n";
        return false;
    }

    // Mở listener
    if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, ListenerCallback, this, &Listener)))
    {
        std::cerr << "ListenerOpen failed\n";
        return false;
    }

    // Bind tất cả IPv4 và IPv6
    QUIC_ADDR addr{};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC); // UNSPEC để dual-stack
    QuicAddrSetPort(&addr, port);

    // Nếu muốn chỉ bind IPv4:
    // QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_INET);
    // QuicAddrSetIpv4Address(&addr, 0); // 0.0.0.0

    // Nếu muốn chỉ bind IPv6:
    // QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_INET6);
    // QuicAddrSetIpv6Address(&addr, 0); // ::

    if (QUIC_FAILED(MsQuic->ListenerStart(Listener, &Alpn, 1, &addr)))
    {
        std::cerr << "ListenerStart failed\n";
        return false;
    }

    std::cout << "QUIC Server running on port " << port << " (0.0.0.0 / ::)...\n";
    return true;
}

// ======================= Stop Server =======================
void QuicServer::Stop()
{
    // Đóng tất cả client còn kết nối
    for (auto &[id, c] : Clients)
    {
        if (c.Stream)
            MsQuic->StreamClose(c.Stream);
        if (c.Connection)
            MsQuic->ConnectionClose(c.Connection);
    }
    Clients.clear();

    // Đóng listener
    if (Listener)
    {
        MsQuic->ListenerClose(Listener);
        Listener = nullptr;
    }

    // Đóng cấu hình
    if (Configuration)
    {
        MsQuic->ConfigurationClose(Configuration);
        Configuration = nullptr;
    }
}

// ======================= Send Message =======================
bool QuicServer::SendMessage(uint64_t connId, const std::string &msg)
{
    auto it = Clients.find(connId);
    if (it == Clients.end() || !it->second.Stream)
        return false;

    QUIC_BUFFER buf;
    buf.Buffer = (uint8_t *)msg.data();
    buf.Length = (uint32_t)msg.size();

    // Gửi message qua QUIC stream
    if (QUIC_FAILED(MsQuic->StreamSend(it->second.Stream, &buf, 1, QUIC_SEND_FLAG_NONE, nullptr)))
    {
        std::cerr << "StreamSend failed\n";
        return false;
    }
    return true;
}

// ======================= Callbacks =======================

// --- Listener Callback ---
// Được gọi khi có kết nối mới tới server
QUIC_STATUS QUIC_API QuicServer::ListenerCallback(HQUIC, void *ctx, QUIC_LISTENER_EVENT *evt)
{
    auto *self = static_cast<QuicServer *>(ctx);
    switch (evt->Type)
    {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION:
    {
        // Gán ID mới cho client
        uint64_t id = self->NextConnId++;
        self->Clients[id] = {evt->NEW_CONNECTION.Connection, nullptr};

        // Gắn callback cho connection
        self->MsQuic->SetCallbackHandler(evt->NEW_CONNECTION.Connection, (void *)ConnectionCallback, self);

        // Áp dụng cấu hình TLS/ALPN
        self->MsQuic->ConnectionSetConfiguration(evt->NEW_CONNECTION.Connection, self->Configuration);

        // Báo cho user code biết có client mới
        if (self->OnClientConnected)
            self->OnClientConnected(id);
        break;
    }
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

// --- Connection Callback ---
// Xử lý sự kiện trên connection
QUIC_STATUS QUIC_API QuicServer::ConnectionCallback(HQUIC conn, void *ctx, QUIC_CONNECTION_EVENT *evt)
{
    auto *self = static_cast<QuicServer *>(ctx);

    // Tìm ID client theo connection handle
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
        // Khi client mở stream -> lưu stream lại và gán callback
        self->Clients[id].Stream = evt->PEER_STREAM_STARTED.Stream;
        self->MsQuic->SetCallbackHandler(evt->PEER_STREAM_STARTED.Stream, (void *)StreamCallback, self);
        break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        // Khi client đóng kết nối
        if (self->OnClientDisconnected)
            self->OnClientDisconnected(id);
        self->Clients.erase(id);
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

// --- Stream Callback ---
// Xử lý dữ liệu trên stream
QUIC_STATUS QUIC_API QuicServer::StreamCallback(HQUIC stream, void *ctx, QUIC_STREAM_EVENT *evt)
{
    auto *self = static_cast<QuicServer *>(ctx);

    // Tìm client theo stream handle
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
        // Nhận dữ liệu từ client
        std::string msg((char *)evt->RECEIVE.Buffers[0].Buffer, evt->RECEIVE.Buffers[0].Length);

        // Gọi callback cho user xử lý message
        if (self->OnMessageReceived)
            self->OnMessageReceived(id, msg);
        break;
    }
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        // Stream đã đóng
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}
