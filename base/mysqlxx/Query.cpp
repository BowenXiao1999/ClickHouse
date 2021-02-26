#if __has_include(<mysql.h>)
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif

#include <Poco/Util/Application.h>

#include <mysqlxx/Connection.h>
#include <mysqlxx/Query.h>
#include <mysqlxx/Types.h>


namespace mysqlxx
{

Query::Query(Connection * conn_, const std::string & query_string) : conn(conn_)
{
    /// Важно в случае, если Query используется не из того же потока, что Connection.
    mysql_thread_init();

    if (!query_string.empty())
        query_buf << query_string;

    query_buf.imbue(std::locale::classic());
}

Query::Query(const Query & other) : conn(other.conn)
{
    /// Важно в случае, если Query используется не из того же потока, что Connection.
    mysql_thread_init();

    query_buf.imbue(std::locale::classic());

    *this << other.str();
}

Query & Query::operator= (const Query & other)
{
    if (this == &other)
        return *this;

    conn = other.conn;

    query_buf.str(other.str());

    return *this;
}

Query::~Query()
{
    mysql_thread_end();
}

void Query::reset()
{
    query_buf.str({});
}

void Query::executeImpl()
{
    std::string query_string = query_buf.str();

    MYSQL* mysql_driver = conn->getDriver();

    auto & logger = Poco::Util::Application::instance().logger();
    logger.trace("Query MySQL server using connection id %lu", mysql_thread_id(mysql_driver));
    if (mysql_real_query(mysql_driver, query_string.data(), query_string.size()))
    {
        const auto errno = mysql_errno(mysql_driver);
        switch (errno)
        {
        case 2006 /* CR_SERVER_GONE_ERROR */:
            [[fallthrough]];
        case 2013 /* CR_SERVER_LOST */:
            throw ConnectionLost(errorMessage(mysql_driver), errno);
        default:
            throw BadQuery(errorMessage(mysql_driver), errno);
        }
    }
}

UseQueryResult Query::use()
{
    executeImpl();
    MYSQL_RES * res = mysql_use_result(conn->getDriver());
    if (!res)
        onError(conn->getDriver());

    return UseQueryResult(res, conn, this);
}

void Query::execute()
{
    executeImpl();
}

UInt64 Query::insertID()
{
    return mysql_insert_id(conn->getDriver());
}

}
