#include <sys/stat.h>
#include "utils.h"
#include "lmdb.h"

#define LMDB_STUDY_DB_PATH "/tmp/lmdb_study"
#define LMDB_STUDY_DB_NAME "lmdb_study"

static bool dir_exists(const char *path)
{
    struct stat info;
    if (stat(path, &info) != 0)
    {
        // stat 失败:路径不存在或无权限
        return false;
    }
    // 检查是否为目录
    return (info.st_mode & S_IFDIR) != 0;
}

void test1(void)
{
    int major, minor, patch;
    mdb_version(&major, &minor, &patch);
    LOG_PRINT_INFO("lmdb version[%d-%d-%d]", major, minor, patch);
    int rc = 0;
    MDB_env *env = NULL;         // 数据库环境句柄
    MDB_dbi dbi = {};            // 数据库标识符(Database Identifier)
    MDB_val key = {}, data = {}; // 键和值的结构体
    MDB_txn *txn = NULL;         // 事务句柄
    MDB_cursor *cursor = NULL;   // 游标, 用于遍历.
    char ckey[32] = {};          // 临时缓冲区, 用于构造键
    char cval[32] = {};          // 临时缓冲区, 用于构造值

    do
    {
        /* Note: Most error checking omitted for simplicity */
        rc = mdb_env_create(&env);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_env_create fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        if (!dir_exists(LMDB_STUDY_DB_PATH))
        {
            mkdir(LMDB_STUDY_DB_PATH, 0755);
        }

        rc = mdb_env_open(env, LMDB_STUDY_DB_PATH, 0, 0664);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_env_open fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        // write begin
        rc = mdb_txn_begin(env, NULL, 0, &txn);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_txn_begin fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        rc = mdb_dbi_open(txn, NULL, 0, &dbi);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_dbi_open fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        key.mv_size = sizeof(int);
        key.mv_data = ckey;
        sprintf(ckey, "%d", 1);

        data.mv_size = sizeof(cval);
        data.mv_data = cval;
        sprintf(cval, "hello world-%d", 32);

        rc = mdb_put(txn, dbi, &key, &data, 0);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_put fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }
        LOG_PRINT_INFO("put key:value[%.*s],value_len[%zu]; data:value[%.*s],value_len[%zu]; success",
                       (int)key.mv_size, (char *)key.mv_data, key.mv_size,
                       (int)data.mv_size, (char *)data.mv_data, key.mv_size);

        data.mv_size = 0;
        data.mv_data = NULL;
        rc = mdb_get(txn, dbi, &key, &data);
        if (MDB_SUCCESS != rc)
        {
            LOG_PRINT_ERROR("mdb_get fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }
        LOG_PRINT_INFO("get key:value[%.*s],value_len[%zu]; data:value[%.*s],value_len[%zu]; success",
                       (int)key.mv_size, (char *)key.mv_data, key.mv_size,
                       (int)data.mv_size, (char *)data.mv_data, key.mv_size);

        // write commit
        rc = mdb_txn_commit(txn);
        if (rc)
        {
            LOG_PRINT_ERROR("mdb_txn_commit fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        // traverse start
        rc = mdb_txn_begin(env, NULL, MDB_RDONLY, &txn);
        if (rc)
        {
            LOG_PRINT_ERROR("mdb_txn_begin fail, ret[%d](%s)", rc, mdb_strerror(rc));
            break;
        }

        rc = mdb_cursor_open(txn, dbi, &cursor);
        while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0)
        {
            LOG_PRINT_INFO("traverse key:pointer[%p],value[%.*s],value_len[%zu]; data:pointer[%p],value[%.*s],value_len[%zu];",
                           key.mv_data, (int)key.mv_size, (char *)key.mv_data, key.mv_size,
                           data.mv_data, (int)data.mv_size, (char *)data.mv_data, key.mv_size);
        }
        // traverse end

    } while (0);

    if (NULL != cursor)
    {
        mdb_cursor_close(cursor);
    }

    if (NULL != txn)
    {
        mdb_txn_abort(txn);
    }

    if (0 != dbi)
    {
        mdb_dbi_close(env, dbi);
    }

    if (NULL != env)
    {
        mdb_env_close(env);
    }
}

int main()
{
    test1();
    return 0;
}