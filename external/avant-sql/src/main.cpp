#include <iostream>
#include <memory>
#include <cstring>

#include "query.h"
#include "connection.h"
#include "sql.h"
#include "pool.h"

using namespace avant::sql;
using std::shared_ptr;

int main(int argc, char **argv)
{
    pool m_pool;
    m_pool.init(10, "IP", "USERNAME", "PASSWORD", "DATABASE");
    {
        shared_ptr<connection> conn = m_pool.get();
        query m_query(conn);
        m_query.select("SELECT * FROM DbMessageData");
        m_pool.back(conn);
    }
    for (int i = 0; i < 10; i++)
    {
        shared_ptr<connection> conn = m_pool.get();

        // SELECT
        sql<1> m_sql(conn, "SELECT GID,Uin,RoleID,SenderName,AttachBlob FROM DbMessageData WHERE GID > ?");

        class DbMessageData
        {
        public:
            DbMessageData() : AttachBlobSize(16777215), AttachBlob(new char[AttachBlobSize])
            {
            }
            ~DbMessageData()
            {
                if (AttachBlob)
                {
                    delete[] AttachBlob;
                }
            }
            int64_t GID;
            int32_t Uin;
            int64_t RoleID;
            char SenderName[64];
            const unsigned int AttachBlobSize;
            char *AttachBlob{nullptr};
        };

        DbMessageData Row;

        int64_t MinGID = 0;
        m_sql.set_bind(0, &MinGID, sizeof(MinGID), value_type::LONGLONG);

        avant::sql::result<5> m_result;
        m_result.set_bind(0, &Row.GID, sizeof(Row.GID), value_type::LONGLONG);
        m_result.set_bind(1, &Row.Uin, sizeof(Row.Uin), value_type::LONG);
        m_result.set_bind(2, &Row.RoleID, sizeof(Row.RoleID), value_type::LONGLONG);
        m_result.set_bind(3, Row.SenderName, sizeof(Row.SenderName), value_type::VAR_STRING);
        m_result.set_bind(4, Row.AttachBlob, Row.AttachBlobSize, value_type::MEDIUM_BLOB);

        int64_t maxGID = 0;
        if (m_sql.execute(m_result))
        {
            while (m_sql.fetch())
            {
                if (!m_result.is_null[0] && maxGID < Row.GID)
                {
                    maxGID = Row.GID;
                }
                if (!m_result.is_null[0] && !m_result.is_null[1] && !m_result.is_null[2] && !m_result.is_null[3] && !m_result.is_null[4])
                {
                    std::cout << "filed len " << m_result.out_length[0] << "," << m_result.out_length[1] << "," << m_result.out_length[2] << "," << m_result.out_length[3] << "," << m_result.out_length[4] << std::endl;
                    std::cout << "GID " << Row.GID << " Uin " << Row.Uin << " RoleID " << Row.RoleID << " SenderName " << std::string(Row.SenderName) << " AttachBlob " << std::string(Row.AttachBlob) << std::endl;
                }
                else
                {
                    std::cout << "have null" << std::endl;
                }
            }
        }

        // UPDATE
        sql<0> m_sql_update(conn, "UPDATE DbMessageData SET Uin=0 WHERE GID > 0");
        avant::sql::result<0> m_update_result;
        if (m_sql_update.execute(m_update_result))
        {
            std::cout << "affected_rows: " << m_sql_update.get_affected_rows() << std::endl;
        }
        else
        {
            std::cout << "m_sql_update.execute failed" << std::endl;
        }

        // DELETE
        sql<0> m_sql_delete(conn, "DELETE FROM DbMessageData WHERE AttachBlob is NULL");
        avant::sql::result<0> m_delete_result;
        if (m_sql_delete.execute(m_delete_result))
        {
            std::cout << "affected_rows: " << m_sql_delete.get_affected_rows() << std::endl;
        }
        else
        {
            std::cout << "m_sql_delete.execute failed" << std::endl;
        }

        // max_allowed_packet
        sql<0> m_sql_11(conn, "SET GLOBAL max_allowed_packet = 67108864000;");
        avant::sql::result<0> m_11_result;
        if (m_sql_11.execute(m_11_result))
        {
            std::cout << "SET GLOBAL max_allowed_packet affected_rows: " << m_sql_11.get_affected_rows() << std::endl;
        }
        else
        {
            std::cout << "m_sql_11.execute failed" << std::endl;
        }

        // INSERT
        sql<17> m_sql_insert(conn, "INSERT INTO DbMessageData(GID,Uin,RoleID,SenderUin,SenderRoleID,SenderName,SendTime,ExpireTime,ReadTime,DeleteTime,Title,Content,Reason,ReasonParam,ExistAttach,AttachFetched,AttachBlob)  VALUES (?,?,?,?,? ,?,?,?,?,? ,?,?,?,?,? ,?,?)");
        int64_t GID = maxGID + 1;
        m_sql_insert.set_bind(0, &GID, sizeof(GID), value_type::LONGLONG);

        int32_t Uin = 0;
        m_sql_insert.set_bind(1, &Uin, sizeof(Uin), value_type::LONG);

        int64_t RoleID = 0;
        m_sql_insert.set_bind(2, &RoleID, sizeof(RoleID), value_type::LONGLONG);

        int32_t SenderUin = 0;
        m_sql_insert.set_bind(3, &SenderUin, sizeof(SenderUin), value_type::LONG);

        int64_t SenderRoleID = 0;
        m_sql_insert.set_bind(4, &SenderRoleID, sizeof(SenderRoleID), value_type::LONGLONG);

        char SenderName[64]{0};
        const char *name = "shadiao";
        strncpy(SenderName, name, strlen(name));
        m_sql_insert.set_bind(5, SenderName, sizeof(SenderName), value_type::VARCHAR, 0);

        int32_t SendTime = 0;
        m_sql_insert.set_bind(6, &SendTime, sizeof(SendTime), value_type::LONG);

        int32_t ExpireTime = 0;
        m_sql_insert.set_bind(7, &ExpireTime, sizeof(ExpireTime), value_type::LONG);

        int32_t ReadTime = 0;
        m_sql_insert.set_bind(8, &ReadTime, sizeof(ReadTime), value_type::LONG);

        int32_t DeleteTime = 0;
        m_sql_insert.set_bind(9, &DeleteTime, sizeof(DeleteTime), value_type::LONG);

        char Title[64]{0};
        m_sql_insert.set_bind(10, Title, sizeof(Title), value_type::VAR_STRING, 0);

        char Content[64]{0};
        m_sql_insert.set_bind(11, Content, sizeof(Content), value_type::STRING, 0);

        int32_t Reason = 0;
        m_sql_insert.set_bind(12, &Reason, sizeof(Reason), value_type::LONG);

        int32_t ReasonParam = 0;
        m_sql_insert.set_bind(13, &ReasonParam, sizeof(ReasonParam), value_type::LONG);

        unsigned char ExistAttach = 1;
        m_sql_insert.set_bind(14, &ExistAttach, sizeof(ExistAttach), value_type::TINY);

        unsigned char AttachFetched = 1;
        m_sql_insert.set_bind(15, &AttachFetched, sizeof(AttachFetched), value_type::TINY);

        char AttachBlob[1]{0};
        m_sql_insert.set_bind(16, AttachBlob, sizeof(AttachBlob), value_type::MEDIUM_BLOB, 0);

        avant::sql::result<0> m_insert_result;
        if (m_sql_insert.execute(m_insert_result))
        {
            std::cout << "affected_rows: " << m_sql_insert.get_affected_rows() << std::endl;
        }
        else
        {
            std::cout << "m_sql_insert.execute failed" << std::endl;
        }

        m_pool.back(conn);
    }
    return 0;
}

// CREATE TABLE DbMessageData (
//     GID BIGINT,
//     Uin INT,
//     RoleID BIGINT,
//     SenderUin INT,
//     SenderRoleID BIGINT,
//     SenderName VARCHAR(64),
//     SendTime INT,
//     ExpireTime INT,
//     ReadTime INT,
//     DeleteTime INT,
//     Title VARCHAR(64),
//     Content VARCHAR(64),
//     Reason INT,
//     ReasonParam INT,
//     ExistAttach TINYINT,
//     AttachFetched TINYINT,
//     AttachBlob MEDIUMBLOB,
//     PRIMARY KEY (GID)
// );