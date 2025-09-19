#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>

struct PGRow
{
    std::vector<std::string> columns;
};

class postgresClient
{
public:
    postgresClient();
    ~postgresClient();

    bool LoadConfig(const std::string &path);
    bool Connect();
    PGresult *Execute(const std::string &query);

    // coroutine query
    boost::asio::awaitable<std::vector<PGRow>> asyncQuery(const std::string &query);

private:
    void BuildConnInfo();

    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    int port_;

    PGconn *conn_;
    std::string conninfo_;

    boost::asio::any_io_executor executor_;
};
