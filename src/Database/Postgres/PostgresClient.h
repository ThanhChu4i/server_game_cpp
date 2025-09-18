#pragma once
#include <string>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <future>
#include <vector>

struct PGRow
{
    std::vector<std::string> columns;
};

class PostgresClient
{
public:
    PostgresClient();
    PostgresClient(const std::string &host,
                   const std::string &user,
                   const std::string &password,
                   const std::string &dbname,
                   int port);
    ~PostgresClient();

    bool Connect();
    PGresult *Execute(const std::string &query);

    // ================= New async function =================
    std::future<std::vector<PGRow>> asyncQuery(const std::string &query);

private:
    PGconn *conn_;
    std::string conninfo_;
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string database_;

    bool LoadConfig(const std::string &path);
    void BuildConnInfo();
};
