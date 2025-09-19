#include "postgresClient.h"
#include <iostream>
#include <fstream>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>

postgresClient::postgresClient()
    : conn_(nullptr), port_(5432), executor_(boost::asio::system_executor())
{
    if (!LoadConfig("database/postgres/config.json"))
    {
        std::cerr << "Failed to load Postgres config!" << std::endl;
    }
    else
    {
        BuildConnInfo();
    }
}

postgresClient::~postgresClient()
{
    if (conn_)
    {
        PQfinish(conn_);
    }
}

bool postgresClient::LoadConfig(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Cannot open Postgres config file: " << path << std::endl;
        return false;
    }

    try
    {
        nlohmann::json j;
        file >> j;
        host_ = j["host"];
        port_ = j["port"];
        user_ = j["user"];
        password_ = j["password"];
        database_ = j["database"];
        std::cout << "Postgres: host: " << host_ << " port: " << port_ << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "Postgres config parse error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void postgresClient::BuildConnInfo()
{
    conninfo_ = "host=" + host_ +
                " port=" + std::to_string(port_) +
                " user=" + user_ +
                " password=" + password_ +
                " dbname=" + database_;
}

bool postgresClient::Connect()
{
    if (conninfo_.empty())
    {
        std::cerr << "Connection info is empty, cannot connect!" << std::endl;
        return false;
    }

    conn_ = PQconnectdb(conninfo_.c_str());

    if (PQstatus(conn_) != CONNECTION_OK)
    {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn_) << std::endl;
        return false;
    }

    std::cout << "Connected to PostgreSQL database successfully!" << std::endl;
    return true;
}

PGresult *postgresClient::Execute(const std::string &query)
{
    if (!conn_)
    {
        std::cerr << "No connection available!" << std::endl;
        return nullptr;
    }
    return PQexec(conn_, query.c_str());
}

// ==================== Coroutine Query ====================
boost::asio::awaitable<std::vector<PGRow>> postgresClient::asyncQuery(const std::string &query)
{
    // run blocking query on a thread pool
    co_return co_await boost::asio::async_initiate<
        decltype(boost::asio::use_awaitable),
        void(std::vector<PGRow>)>(
        [this, query](auto handler)
        {
            // chuyển sang 1 thread khác để không block io_context
            std::thread([this, query, handler = std::move(handler)]() mutable
                        {
                std::vector<PGRow> rows;

                if (!conn_) {
                    std::cerr << "No connection available!" << std::endl;
                    boost::asio::post(executor_, [h = std::move(handler), rows = std::move(rows)]() mutable {
                        h(std::move(rows));
                    });
                    return;
                }

                PGresult* res = PQexec(conn_, query.c_str());
                if (!res) {
                    std::cerr << "Query failed!" << std::endl;
                    boost::asio::post(executor_, [h = std::move(handler), rows = std::move(rows)]() mutable {
                        h(std::move(rows));
                    });
                    return;
                }

                if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                    std::cerr << "Query error: " << PQresultErrorMessage(res) << std::endl;
                    PQclear(res);
                    boost::asio::post(executor_, [h = std::move(handler), rows = std::move(rows)]() mutable {
                        h(std::move(rows));
                    });
                    return;
                }

                int nRows = PQntuples(res);
                int nCols = PQnfields(res);

                for (int i = 0; i < nRows; ++i) {
                    PGRow row;
                    for (int j = 0; j < nCols; ++j) {
                        char* val = PQgetvalue(res, i, j);
                        row.columns.push_back(val ? val : "");
                    }
                    rows.push_back(std::move(row));
                }

                PQclear(res);

                // chuyển kết quả về lại executor chính
                boost::asio::post(executor_, [h = std::move(handler), rows = std::move(rows)]() mutable {
                    h(std::move(rows));
                }); })
                .detach();
        },
        boost::asio::use_awaitable);
}
