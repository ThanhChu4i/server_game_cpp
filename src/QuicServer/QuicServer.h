#pragma once
#include <msquic.h>
#include <string>
#include <functional>

/// @brief Lớp QuicServer bao bọc MsQuic API theo phong cách OOP.
/// - Chịu trách nhiệm khởi tạo, chạy và dừng QUIC server.
/// - Quản lý nhiều client (connection/stream).
/// - Cung cấp callback để ứng dụng xử lý sự kiện (connect/disconnect/message).
class QuicServer
{
public:
    /// @brief Thông tin về 1 client (connection + stream + auth + playerId)
    struct Client
    {
        HQUIC Connection{nullptr}; ///< QUIC connection handle
        HQUIC Stream{nullptr};     ///< QUIC stream mặc định
        bool auth{false};          ///< Trạng thái đã auth chưa
        std::string playerId{"0"}; ///< UUID lưu dưới dạng string
    };

public:
    /// @brief Constructor
    /// @param certFile đường dẫn tới file certificate (PEM)
    /// @param keyFile đường dẫn tới file private key (PEM)
    QuicServer(const std::string &certFile, const std::string &keyFile);

    /// @brief Destructor
    ~QuicServer();

    /// @brief Bắt đầu server, bind vào port chỉ định.
    /// @param port UDP port (thường >=1024 nếu không chạy sudo).
    /// @return true nếu thành công, false nếu thất bại.
    bool Start(uint16_t port);

    /// @brief Dừng server, giải phóng connection, stream, listener, config.
    void Stop();

    /// @brief Gửi message tới client theo connId.
    /// @param connId ID client đích.
    /// @param msg dữ liệu gửi.
    /// @return true nếu gửi thành công, false nếu thất bại.
    bool SendMessage(uint64_t connId, const std::string &msg);

    /// @brief Lấy con trỏ tới Client theo connId
    Client *GetClient(uint64_t connId)
    {
        auto it = Clients.find(connId);
        if (it != Clients.end())
            return &(it->second);
        return nullptr;
    }

    // ========== Callback do người dùng định nghĩa ==========
    std::function<void(uint64_t connId)> OnClientConnected;
    std::function<void(uint64_t connId)> OnClientDisconnected;
    std::function<void(uint64_t connId, const std::string &msg)> OnMessageReceived;

private:
    // ========== Các resource chính của MsQuic ==========
    const QUIC_API_TABLE *MsQuic{nullptr}; ///< Bảng function pointer của MsQuic
    HQUIC Registration{nullptr};           ///< Đăng ký ứng dụng
    HQUIC Configuration{nullptr};          ///< TLS/ALPN config
    HQUIC Listener{nullptr};               ///< Listener accept connections

    std::unordered_map<uint64_t, Client> Clients; ///< Danh sách client
    uint64_t NextConnId{1};                       ///< Counter ID client
    std::string CertFile;
    std::string KeyFile;

    // ========== Các callback static (MsQuic yêu cầu) ==========
    static QUIC_STATUS QUIC_API ListenerCallback(HQUIC, void *, QUIC_LISTENER_EVENT *);
    static QUIC_STATUS QUIC_API ConnectionCallback(HQUIC, void *, QUIC_CONNECTION_EVENT *);
    static QUIC_STATUS QUIC_API StreamCallback(HQUIC, void *, QUIC_STREAM_EVENT *);
};
