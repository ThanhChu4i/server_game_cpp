#include "PostgresClient.h"
#include <iostream>
#include <fstream>
#include <thread>

PostgresClient::PostgresClient() : conn_(nullptr), port_(5432)
{
    if (!LoadConfig("Database/Postgres/config.json"))
    {
        std::cerr << "Failed to load Postgres config!" << std::endl;
    }
    else
    {
        BuildConnInfo();
    }
}

PostgresClient::PostgresClient(const std::string &host,
                               const std::string &user,
                               const std::string &password,
                               const std::string &dbname,
                               int port)
    : host_(host), user_(user), password_(password), database_(dbname), port_(port), conn_(nullptr)
{
    BuildConnInfo();
}

PostgresClient::~PostgresClient()
{
    if (conn_ != nullptr)
    {
        PQfinish(conn_);
    }
}

bool PostgresClient::LoadConfig(const std::string &path)
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

void PostgresClient::BuildConnInfo()
{
    conninfo_ = "host=" + host_ +
                " port=" + std::to_string(port_) +
                " user=" + user_ +
                " password=" + password_ +
                " dbname=" + database_;
}

bool PostgresClient::Connect()
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

PGresult *PostgresClient::Execute(const std::string &query)
{
    if (!conn_)
    {
        std::cerr << "No connection available!" << std::endl;
        return nullptr;
    }

    return PQexec(conn_, query.c_str());
}

// ================= New async function =================
std::future<std::vector<PGRow>> PostgresClient::asyncQuery(const std::string &query)
{
    auto promise = std::make_shared<std::promise<std::vector<PGRow>>>();
    auto fut = promise->get_future();

    std::thread([this, query, promise]()
                {
                    std::vector<PGRow> rows;

                    if (!conn_)
                    {
                        std::cerr << "No connection available!" << std::endl;
                        promise->set_value(rows);
                        return;
                    }

                    PGresult *res = PQexec(conn_, query.c_str());
                    if (res == nullptr)
                    {
                        std::cerr << "Query failed!" << std::endl;
                        promise->set_value(rows);
                        return;
                    }

                    if (PQresultStatus(res) != PGRES_TUPLES_OK)
                    {
                        std::cerr << "Query error: " << PQresultErrorMessage(res) << std::endl;
                        PQclear(res);
                        promise->set_value(rows);
                        return;
                    }

                    int nRows = PQntuples(res);
                    int nCols = PQnfields(res);

                    for (int i = 0; i < nRows; ++i)
                    {
                        PGRow row;
                        for (int j = 0; j < nCols; ++j)
                        {
                            char *val = PQgetvalue(res, i, j);
                            row.columns.push_back(val ? val : "");
                        }
                        rows.push_back(row);
                    }

                    PQclear(res);

                    // Set kết quả vào promise
                    promise->set_value(rows); })
        .detach();

    return fut;
}
