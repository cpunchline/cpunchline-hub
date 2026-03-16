#include <float.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include "utility/utils.h"
#include "sqlite3.h"

// sqlite3 是一个读并发, 写单线程的, 带有库锁的数据库;

#define DB_KEY_LEN   (1024)
#define DB_VALUE_LEN (4096)

#define DB_PATH        "/tmp"
#define DB_BACKUP_PATH "/tmp" // you can change it
#define DB_FILE        DB_PATH "/setting.db"
#define DB_BACKUP_FILE DB_BACKUP_PATH "/setting_bak.db"

#define DB_DEFAULT_JSON_FILE DB_PATH "/db_default.json"
#define DB_UPDATE_JSON_FILE  DB_PATH "/db_update.json"

#define SQL_CMD_BUF_SIZE (PATH_MAX)

#define DB_LIB_VERSION "SELECT SQLITE_VERSION();"

#define SQL_CREATE_TABLE         "CREATE TABLE IF NOT EXISTS setting_table (key TEXT PRIMARY KEY, type NUMERIC, value_len NUMERIC, value TEXT, min TEXT, max TEXT, defaults TEXT, security NUMERIC);"
#define SQL_INSERT_TABLE         "INSERT INTO setting_table (key, type, value_len, value, min, max, defaults, security) VALUES(?,?,?,?,?,?,?,?);"
#define SQL_INSERT_IGNORE_TABLE  "INSERT OR IGNORE INTO setting_table (key, value_len, type, value, min, max, defaults, security) VALUES(?,?,?,?,?,?,?,?);"  // only insert once
#define SQL_INSERT_REPLACE_TABLE "INSERT OR REPLACE INTO setting_table (key, type, value_len, value, min, max, defaults, security) VALUES(?,?,?,?,?,?,?,?);" // insert and replace
#define SQL_DELETE_TABLE         "DELETE FROM setting_table WHERE key=?;"
#define SQL_UPDATE_TABLE         "UPDATE setting_table SET value=? WHERE key=?;"
#define SQL_RESET_TABLE          "UPDATE setting_table SET value=defaults WHERE key=?;"
#define SQL_SELECT_TABLE         "SELECT * FROM setting_table WHERE key=?;"
#define SQL_COUNT_TABLE          "SELECT count(*) FROM setting_table;"
#define SQL_JUDGEKEY_TABLE       "SELECT count(*) FROM setting_table WHERE key=?;"

#define SQL_BEGIN_TRANSACTION        "BEGIN TRANSACTION;"
#define SQL_END_COMMIT_TRANSACTION   "COMMIT;"
#define SQL_END_ROLLBACK_TRANSACTION "ROLLBACK;"

#define SQL_INTEGRITY_CHECK_TABLE "PRAGMA integrity_check;"
#define SQL_JOURNAL_MODE_WAL      "PRAGMA journal_mode=WAL;"
#define SQL_SYNCHRONOUS_FULL      "PRAGMA synchronous=FULL;"
// OFF   : 不调用 fsync; 掉电可能丢大量数据; 适合批处理,缓存,不重要数据;
// NORMAL: WAL 层 fsync 延迟到后台; 掉电可能丢"最后一次提交"; 推荐一般业务;
// FULL  : 每次事务提交都会 fsync; 传统,稳健;TP 型业务常用;
// EXTRA : FULL + metadata fsync; 确保文件元数据一致; 几乎坚不可摧;

#define SQL_JOURNAL_SIZE_LIMIT "PRAGMA journal_size_limit=%d;"
#define SQL_WAL_AUTOCHECKPOINT "PRAGMA wal_autocheckpoint=%d;" // 配置数据页面达到指定值进行落盘;

#define DB_START_TRANSACTION(db_handle)                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        int32_t iret = sqlite3_exec(db_handle, SQL_BEGIN_TRANSACTION, 0, 0, NULL);                                     \
        if (iret != SQLITE_OK)                                                                                         \
        {                                                                                                              \
            LOG_PRINT_ERROR("sqlite3_exec COMMIT START fail, ret[%d], errcode[%d]", iret, sqlite3_errcode(db_handle)); \
        }                                                                                                              \
    } while (0)

#define DB_END_TRANSACTION(db_handle)                                                                                \
    do                                                                                                               \
    {                                                                                                                \
        int32_t iret = sqlite3_exec(db_handle, SQL_END_COMMIT_TRANSACTION, 0, 0, NULL);                              \
        if (iret != SQLITE_OK)                                                                                       \
        {                                                                                                            \
            LOG_PRINT_ERROR("sqlite3_exec COMMIT END fail, ret[%d], errcode[%d]", iret, sqlite3_errcode(db_handle)); \
        }                                                                                                            \
    } while (0)

#define DB_ROLLBACK_TRANSACTION(db_handle)                                                                                \
    do                                                                                                                    \
    {                                                                                                                     \
        int32_t iret = sqlite3_exec(db_handle, SQL_END_ROLLBACK_TRANSACTION, 0, 0, NULL);                                 \
        if (iret != SQLITE_OK)                                                                                            \
        {                                                                                                                 \
            LOG_PRINT_ERROR("sqlite3_exec COMMIT ROLLBACK fail, ret[%d], errcode[%d]", iret, sqlite3_errcode(db_handle)); \
        }                                                                                                                 \
    } while (0)

typedef enum _db_insert_type_e
{
    E_DB_INSERT_TYPE_ONLY, // defaults
    E_DB_INSERT_TYPE_IGNORE,
    E_DB_INSERT_TYPE_REPLACE,
    E_DB_INSERT_TYPE_MAX
} db_insert_type_e;

typedef enum _db_value_type_e
{
    // TEXT
    E_DB_VALUE_TYPE_BOOL,   /* bool */
    E_DB_VALUE_TYPE_INT,    /* int64 */
    E_DB_VALUE_TYPE_DOUBLE, /* double */
    E_DB_VALUE_TYPE_STRING, /* string */
    E_DB_VALUE_TYPE_BLOB,   /* array */
    E_DB_VALUE_TYPE_INTBLOB /* int array */
} db_value_type_e;

typedef struct _db_item_t
{
    char key[DB_KEY_LEN];
    int type;
    int value_len;
    char value[DB_VALUE_LEN];
    char min[DB_VALUE_LEN];
    char max[DB_VALUE_LEN];
    char defaults[DB_VALUE_LEN];
    int security;
} db_item_t;

typedef enum
{
    E_ITEM_IN_1 = 1,
    E_ITEM_IN_2,
    E_ITEM_IN_3,
    E_ITEM_IN_4,
    E_ITEM_IN_5,
    E_ITEM_IN_6,
    E_ITEM_IN_7,
    E_ITEM_IN_8
} db_item_in_e;

typedef enum
{
    E_ITEM_OUT_1 = 0,
    E_ITEM_OUT_2,
    E_ITEM_OUT_3,
    E_ITEM_OUT_4,
    E_ITEM_OUT_5,
    E_ITEM_OUT_6,
    E_ITEM_OUT_7,
    E_ITEM_OUT_8
} db_item_out_e;

static db_item_t g_default_db_item_array[] = {
    // key         type                  value_len        value        min                  max          defaults       security
    {"DB_VERSION", E_DB_VALUE_TYPE_INT,  sizeof(int64_t), "1", "-9223372036854775807", "9223372036854775807", "1", 0},
    {"DB_END",     E_DB_VALUE_TYPE_BOOL, sizeof(int64_t), "1", "0",                    "1",                   "1", 0},
};
static size_t g_default_db_item_array_size = UTIL_ARRAY_SIZE(g_default_db_item_array);
static sqlite3 *db_handle = NULL;

#if 0
static int32_t db_query_sqlite_version()
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    const unsigned char *result = NULL;

    do
    {

        sql_ret = sqlite3_prepare_v3(db_handle, DB_LIB_VERSION, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_ROW != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        result = sqlite3_column_text(stmt, 0);
        if (NULL == result)
        {
            LOG_PRINT_ERROR("sqlite3_column_text fail, sql_ret[%d]", sql_ret);
            ret = -1;
            break;
        }

        LOG_PRINT_INFO("sqlite version[%s]", result);
        ret = 0;
        break;
    } while (0);

    if (NULL != stmt)
    {
        sqlite3_finalize(stmt);
    }

    return ret;
}
#endif

static int32_t db_integrity_check(const char *db_file)
{
    if (NULL == db_file)
    {
        return -1;
    }

    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    const unsigned char *result = NULL;
    sqlite3 *tmp_db_handle = NULL;

    do
    {
        sql_ret = sqlite3_open_v2(db_file, &tmp_db_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_open_v2 fail, sql_ret[%d]", sql_ret);
            ret = -1;
            break;
        }

        sql_ret = sqlite3_prepare_v3(tmp_db_handle, SQL_INTEGRITY_CHECK_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(tmp_db_handle));
            ret = -1;
            break;
        }

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_ROW != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(tmp_db_handle));
            ret = -1;
            break;
        }

        result = sqlite3_column_text(stmt, 0);
        if (NULL == result)
        {
            LOG_PRINT_ERROR("sqlite3_column_text fail, sql_ret[%d]", sql_ret);
            ret = -1;
            break;
        }

        if (0 != strcmp((const char *)result, "ok"))
        {
            LOG_PRINT_ERROR("db file not ok, damage!");
            ret = -1;
            break;
        }

        ret = 0;
        break;
    } while (0);

    if (NULL != stmt)
    {
        sqlite3_finalize(stmt);
    }

    if (NULL != tmp_db_handle)
    {
        sqlite3_close(tmp_db_handle);
    }

    if (ret != 0)
    {
        LOG_PRINT_WARN("db should recovery from backup.");
    }

    return ret;
}

static int32_t db_table_is_exist(void)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;

    do
    {
        sql_ret = sqlite3_prepare_v3(db_handle, SQL_COUNT_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_ROW != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        int table_count = sqlite3_column_int(stmt, 0);
        LOG_PRINT_INFO("table count[%d]", table_count);
        ret = 0;
        break;
    } while (0);

    if (NULL != stmt)
    {
        sqlite3_finalize(stmt);
    }

    return ret;
}

static int32_t db_open(const char *db_file)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    char sql_cmd[SQL_CMD_BUF_SIZE] = {};
    int32_t retry_times = 3;

    do
    {
        // 数据库初始化设置
        // 1. 持久化有效;   数据库存储文本编码                PRAGMA encoding=UTF-8; (默认UTF-8); 只能在创建 DB 后, 建表前设置, 且为空库可以设置;
        // 2. 持久化有效;   数据库页面大小                    PRAGMA page_size=4096; (默认4096); 只能在建表前设置;
        // 3. 持久化有效;   数据库文件身份标识                PRAGMA application_id=0;(默认0);
        // 4. 持久化有效;   写入文件头, 设置数据库schema版本   PRAGMA user_version=0;(默认0);
        // 5. 持久化有效;   控制SQLite是否进行磁盘空间的回收   PRAGMA auto_vacuum=FULL; (默认NONE); NONE不自动回收/FULL删除数据立即收缩文件/INCREMENTAL手动收缩空间 PRAGMA incremental_vacuum;(持久化有效;))
        // 6. 单次连接有效; 设置同步级别(磁盘安全等级)         PRAGMA synchronous=FULL; (默认FULL);
        // 7. 持久化有效;                                    PRAGMA journal_mode=WAL; (默认memory); 读写都会有数据库锁; wal则仅写数据库锁;
        // 8. 单次连接有效; WAL 模式下自动 checkpoint间隔     PRAGMA wal_autocheckpoint=200;(默认1000)

        sql_ret = sqlite3_open_v2(db_file, &db_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_open_v2 fail, sql_ret[%d]", sql_ret);
            ret = -1;
            break;
        }

        sql_ret = sqlite3_exec(db_handle, SQL_JOURNAL_MODE_WAL, NULL, 0, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_exec fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        sql_ret = sqlite3_exec(db_handle, SQL_SYNCHRONOUS_FULL, NULL, 0, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_exec fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        int journal_size_limit = DB_VALUE_LEN * 100; // 100KB; 设置 回滚日志(rollback journal)或 WAL 文件的最大大小限制;
        memset(sql_cmd, 0x00, sizeof(sql_cmd));
        snprintf(sql_cmd, sizeof(sql_cmd), SQL_JOURNAL_SIZE_LIMIT, journal_size_limit);
        sql_ret = sqlite3_exec(db_handle, sql_cmd, NULL, 0, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_exec fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        int wal_autocheckpoint = 1000; // 约4MB落盘一次;
        memset(sql_cmd, 0x00, sizeof(sql_cmd));
        snprintf(sql_cmd, sizeof(sql_cmd), SQL_WAL_AUTOCHECKPOINT, wal_autocheckpoint);
        sql_ret = sqlite3_exec(db_handle, sql_cmd, NULL, 0, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_exec fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }
        ret = 0;
    } while (retry_times-- > 0 && ret != 0);

    if (0 != ret && NULL != db_handle)
    {
        sqlite3_close_v2(db_handle);
        db_handle = NULL;
    }

    return ret;
}

static int32_t db_create(const char *db_file)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;

    do
    {
        if (NULL != db_file)
        {
            remove(db_file);
            sqlite3_close_v2(db_handle);
            db_handle = NULL;
        }

        ret = db_open(db_file);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("db_open fail, ret[%d]", ret);
            break;
        }

        sql_ret = sqlite3_exec(db_handle, SQL_CREATE_TABLE, NULL, 0, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_exec fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        ret = 0;
    } while (0);

    return ret;
}

static int32_t db_insert(bool is_commit, db_insert_type_e insert_mode, const char *key, db_value_type_e type, const void *cfg_data, size_t len)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    int64_t int64_value = 0;
    double double_value = 0;
    char string_value[DB_VALUE_LEN] = {};
    const char *insert_sql_cmd = NULL;

    do
    {
        if (NULL == key || NULL == cfg_data || strlen(key) > DB_KEY_LEN || len > DB_VALUE_LEN)
        {
            LOG_PRINT_ERROR("invalid params!");
            ret = -1;
            break;
        }

        if (E_DB_INSERT_TYPE_IGNORE == insert_mode)
        {
            insert_sql_cmd = SQL_INSERT_IGNORE_TABLE;
        }
        else if (E_DB_INSERT_TYPE_REPLACE == insert_mode)
        {
            insert_sql_cmd = SQL_INSERT_REPLACE_TABLE;
        }
        else
        {
            insert_sql_cmd = SQL_INSERT_TABLE;
        }

        if (is_commit)
        {
            DB_START_TRANSACTION(db_handle);
        }

        sql_ret = sqlite3_prepare_v3(db_handle, insert_sql_cmd, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_1, key, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, E_ITEM_IN_2, (int)type);
        sqlite3_bind_int(stmt, E_ITEM_IN_3, (int)len);

        switch (type)
        {
            case E_DB_VALUE_TYPE_BOOL:
                memcpy(string_value, cfg_data, len);
                sscanf(string_value, "%" PRIu64, &int64_value);
                if (int64_value != 0)
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_4, 1);
                    sqlite3_bind_int64(stmt, E_ITEM_IN_7, 1);
                }
                else
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_4, 0);
                    sqlite3_bind_int64(stmt, E_ITEM_IN_7, 0);
                }
                sqlite3_bind_int64(stmt, E_ITEM_IN_5, 0);
                sqlite3_bind_int64(stmt, E_ITEM_IN_6, 1);
                break;
            case E_DB_VALUE_TYPE_INT:
                memcpy(string_value, cfg_data, len);
                sscanf(string_value, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_4, int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_5, INT64_MIN);
                sqlite3_bind_int64(stmt, E_ITEM_IN_6, INT64_MAX);
                sqlite3_bind_int64(stmt, E_ITEM_IN_7, int64_value);
                break;
            case E_DB_VALUE_TYPE_DOUBLE:
                double_value = atof(cfg_data);
                sqlite3_bind_double(stmt, E_ITEM_IN_4, double_value);
                sqlite3_bind_double(stmt, E_ITEM_IN_5, DBL_MIN);
                sqlite3_bind_double(stmt, E_ITEM_IN_6, DBL_MAX);
                sqlite3_bind_double(stmt, E_ITEM_IN_7, double_value);
                break;
            case E_DB_VALUE_TYPE_STRING:
                memcpy(string_value, cfg_data, len);
                sqlite3_bind_text(stmt, E_ITEM_IN_4, string_value, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, E_ITEM_IN_7, string_value, -1, SQLITE_STATIC);
                break;
            case E_DB_VALUE_TYPE_BLOB:
            case E_DB_VALUE_TYPE_INTBLOB:
                sqlite3_bind_blob(stmt, E_ITEM_IN_4, (uint8_t *)cfg_data, (int)len, SQLITE_STATIC);
                sqlite3_bind_blob(stmt, E_ITEM_IN_7, (uint8_t *)cfg_data, (int)len, SQLITE_STATIC);
                break;
            default:
                LOG_PRINT_ERROR("invalid type[%d]", type);
                if (is_commit)
                {
                    DB_ROLLBACK_TRANSACTION(db_handle);
                }
                sqlite3_finalize(stmt);
                return -1;
        }
        sqlite3_bind_int(stmt, E_ITEM_IN_8, 0); // no security

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            if (is_commit)
            {
                DB_ROLLBACK_TRANSACTION(db_handle);
            }
            sqlite3_finalize(stmt);
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_END_TRANSACTION(db_handle);
        }
        sqlite3_finalize(stmt);

        LOG_PRINT_INFO("insert key[%s] ok!", key);

        ret = 0;
    } while (0);

    return ret;
}

static int32_t db_delete(bool is_commit, const char *key)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;

    do
    {
        if (NULL == key || strlen(key) > DB_KEY_LEN)
        {
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_START_TRANSACTION(db_handle);
        }

        sql_ret = sqlite3_prepare_v3(db_handle, SQL_DELETE_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_1, key, -1, SQLITE_STATIC);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            if (is_commit)
            {
                DB_ROLLBACK_TRANSACTION(db_handle);
            }
            sqlite3_finalize(stmt);
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_END_TRANSACTION(db_handle);
        }
        sqlite3_finalize(stmt);

        ret = 0;
    } while (0);

    return ret;
}

static bool db_check_item_is_exist(const char *key)
{
    bool exist_flag = false;
    int32_t sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    int num = 0;

    do
    {
        if (NULL == key || strlen(key) > DB_KEY_LEN)
        {
            LOG_PRINT_ERROR("invalid params!");
            exist_flag = false;
            break;
        }

        sql_ret = sqlite3_prepare_v3(db_handle, SQL_JUDGEKEY_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            exist_flag = false;
            break;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_1, key, -1, SQLITE_STATIC);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_ROW != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            sqlite3_finalize(stmt);
            exist_flag = false;
            break;
        }

        num = sqlite3_column_int(stmt, E_ITEM_OUT_1);
        if (0 == num)
        {
            exist_flag = false;
        }
        else
        {
            exist_flag = true;
        }
        sqlite3_finalize(stmt);

    } while (0);

    return exist_flag;
}

static bool db_check_item_is_overflow(const char *key, int32_t type, const void *cfg_data, size_t len)
{
    bool out_range = false;
    char string_value[DB_VALUE_LEN] = {};
    int64_t int64_value = 0;
    int64_t min_int64_value = 0;
    int64_t max_int64_value = 0;

    double double_value = 0;
    double min_double_value = 0;
    double max_double_value = 0;

    if (type == E_DB_VALUE_TYPE_BOOL || type == E_DB_VALUE_TYPE_INT)
    {
        if (len != sizeof(int64_t))
        {
            return false;
        }
    }
    else if (type == E_DB_VALUE_TYPE_DOUBLE)
    {
        if (len != sizeof(double))
        {
            return false;
        }
    }

    memcpy(string_value, cfg_data, len);
    for (size_t i = 0; i < g_default_db_item_array_size; ++i)
    {
        if (0 == strcmp(key, g_default_db_item_array[i].key) && type == g_default_db_item_array[i].type)
        {
            if (g_default_db_item_array[i].type == E_DB_VALUE_TYPE_INT)
            {
                sscanf(string_value, "%" PRIu64, &int64_value);
                sscanf(g_default_db_item_array[i].min, "%" PRIu64, &min_int64_value);
                sscanf(g_default_db_item_array[i].max, "%" PRIu64, &max_int64_value);

                if (int64_value < min_int64_value || int64_value > max_int64_value)
                {
                    out_range = true;
                }
            }
            else if (g_default_db_item_array[i].type == E_DB_VALUE_TYPE_DOUBLE)
            {
                double_value = atof(string_value);
                min_double_value = atof(g_default_db_item_array[i].min);
                max_double_value = atof(g_default_db_item_array[i].max);

                if (double_value < min_double_value || double_value > max_double_value)
                {
                    out_range = true;
                }
            }
            break;
        }
    }

    return out_range;
}

static int32_t db_update(bool is_commit, const char *key, db_value_type_e type, const void *cfg_data, size_t len)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    int64_t int64_value = 0;
    double double_value = 0;
    char string_value[DB_VALUE_LEN] = {};

    do
    {
        if (NULL == key || NULL == cfg_data || strlen(key) > DB_KEY_LEN || len > DB_VALUE_LEN)
        {
            LOG_PRINT_ERROR("invalid params!");
            ret = -1;
            break;
        }

        if (!db_check_item_is_exist(key))
        {
            LOG_PRINT_ERROR("key[%s] not exist!", key);
            ret = -1;
            break;
        }

        if (type == E_DB_VALUE_TYPE_INT || type == E_DB_VALUE_TYPE_DOUBLE)
        {
            if (db_check_item_is_overflow(key, (int32_t)type, cfg_data, len))
            {
                LOG_PRINT_ERROR("key[%s] value out of range!", key);
                ret = -1;
                break;
            }
        }

        if (is_commit)
        {
            DB_START_TRANSACTION(db_handle);
        }

        sql_ret = sqlite3_prepare_v3(db_handle, SQL_UPDATE_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }

        switch (type)
        {
            case E_DB_VALUE_TYPE_BOOL:
                memcpy(string_value, cfg_data, len);
                sscanf(string_value, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_1, int64_value);
                break;
            case E_DB_VALUE_TYPE_INT:
                memcpy(string_value, cfg_data, len);
                sscanf(string_value, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_1, int64_value);
                break;
            case E_DB_VALUE_TYPE_DOUBLE:
                double_value = atof(cfg_data);
                sqlite3_bind_double(stmt, E_ITEM_IN_1, double_value);
                break;
            case E_DB_VALUE_TYPE_STRING:
                memcpy(string_value, cfg_data, len);
                sqlite3_bind_text(stmt, E_ITEM_IN_1, string_value, -1, NULL);
                break;
            case E_DB_VALUE_TYPE_BLOB:
            case E_DB_VALUE_TYPE_INTBLOB:
                sqlite3_bind_blob(stmt, E_ITEM_IN_1, (uint8_t *)cfg_data, (int)len, NULL);
                break;
            default:
                LOG_PRINT_ERROR("invalid type[%d]", type);
                if (is_commit)
                {
                    DB_ROLLBACK_TRANSACTION(db_handle);
                }
                sqlite3_finalize(stmt);
                return -1;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_2, key, -1, NULL);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            if (is_commit)
            {
                DB_ROLLBACK_TRANSACTION(db_handle);
            }
            sqlite3_finalize(stmt);
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_END_TRANSACTION(db_handle);
        }
        sqlite3_finalize(stmt);

        LOG_PRINT_INFO("update key[%s] ok!", key);

        ret = 0;
    } while (0);

    return ret;
}

static int32_t db_update_reset(bool is_commit, const char *key)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;

    do
    {
        if (NULL == key || strlen(key) > DB_KEY_LEN)
        {
            LOG_PRINT_ERROR("invalid params!");
            ret = -1;
            break;
        }

        if (!db_check_item_is_exist(key))
        {
            LOG_PRINT_ERROR("key[%s] not exist!", key);
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_START_TRANSACTION(db_handle);
        }

        sql_ret = sqlite3_prepare_v3(db_handle, SQL_RESET_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_1, key, -1, NULL);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            if (is_commit)
            {
                DB_ROLLBACK_TRANSACTION(db_handle);
            }
            sqlite3_finalize(stmt);
            ret = -1;
            break;
        }

        if (is_commit)
        {
            DB_END_TRANSACTION(db_handle);
        }
        sqlite3_finalize(stmt);

        LOG_PRINT_INFO("reset key[%s] to default ok!", key);

        ret = 0;
    } while (0);

    return ret;
}

static int32_t db_select(const char *key, db_value_type_e type, void *cfg_data, size_t *len)
{
    int32_t ret = 0;
    int sql_ret = SQLITE_OK;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    int cfg_type = 0;
    int cfg_size = 0;
    int64_t int64_value = 0;
    double double_value = 0;
    const unsigned char *uint8_pvalue = NULL;

    do
    {
        if (NULL == key || NULL == cfg_data || NULL == len || strlen(key) > DB_KEY_LEN)
        {
            LOG_PRINT_ERROR("invalid params!");
            ret = -1;
            break;
        }

        sql_ret = sqlite3_prepare_v3(db_handle, SQL_SELECT_TABLE, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            ret = -1;
            break;
        }
        sqlite3_bind_text(stmt, E_ITEM_IN_1, key, -1, SQLITE_STATIC);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_ROW != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", key, sql_ret, sqlite3_errcode(db_handle));
            sqlite3_finalize(stmt);
            ret = -1;
            break;
        }

        cfg_type = sqlite3_column_int(stmt, E_ITEM_OUT_2);
        switch (cfg_type)
        {
            case E_DB_VALUE_TYPE_BOOL:
                int64_value = sqlite3_column_int64(stmt, E_ITEM_OUT_4);
                cfg_size = sqlite3_column_int(stmt, E_ITEM_OUT_3);
                *len = (size_t)cfg_size;
                if (1 == int64_value)
                {
                    *(bool *)cfg_data = true;
                }
                else
                {
                    *(bool *)cfg_data = false;
                }
                break;
            case E_DB_VALUE_TYPE_INT:
                int64_value = sqlite3_column_int64(stmt, E_ITEM_OUT_4);
                cfg_size = sqlite3_column_int(stmt, E_ITEM_OUT_3);
                *len = (size_t)cfg_size;
                sprintf((char *)cfg_data, "%" PRIu64, int64_value);
                break;
            case E_DB_VALUE_TYPE_DOUBLE:
                double_value = sqlite3_column_double(stmt, E_ITEM_OUT_4);
                cfg_size = sqlite3_column_int(stmt, E_ITEM_OUT_3);
                *len = (size_t)cfg_size;
                sprintf((char *)cfg_data, "%lf", double_value);
                break;
            case E_DB_VALUE_TYPE_STRING:
                uint8_pvalue = sqlite3_column_text(stmt, E_ITEM_OUT_4);
                cfg_size = sqlite3_column_int(stmt, E_ITEM_OUT_3);
                if (cfg_size <= DB_VALUE_LEN)
                {
                    *len = (size_t)cfg_size;
                    memcpy(cfg_data, uint8_pvalue, *len);
                }
                else
                {
                    LOG_PRINT_ERROR("key[%s]-type[%d]-value[%s] too long, truncate it!", key, type, uint8_pvalue);
                }
                break;
            case E_DB_VALUE_TYPE_BLOB:
            case E_DB_VALUE_TYPE_INTBLOB:
                uint8_pvalue = sqlite3_column_blob(stmt, E_ITEM_OUT_4);
                cfg_size = sqlite3_column_int(stmt, E_ITEM_OUT_3);
                if (cfg_size <= DB_VALUE_LEN)
                {
                    *len = (size_t)cfg_size;
                    memcpy(cfg_data, uint8_pvalue, *len);
                }
                else
                {
                    LOG_PRINT_ERROR("key[%s]-type[%d], blob/intblob value too long, truncate it!", key, type);
                }
                break;
            default:
                LOG_PRINT_ERROR("invalid type[%d]", cfg_type);
                return -1;
        }

        sqlite3_finalize(stmt);

        LOG_PRINT_INFO("select key[%s] ok!", key);

        ret = 0;
    } while (0);

    return ret;
}

static int32_t db_load(db_insert_type_e insert_mode, db_item_t *item_array, size_t item_array_size)
{
    int32_t ret = 0;
    int32_t sql_ret = 0;
    sqlite3_stmt *stmt = NULL;
    const char *ztail = NULL;
    int64_t int64_value = 0;
    double double_value = 0;
    const char *insert_sql_cmd = NULL;

    if (NULL == item_array || 0 == item_array_size)
    {
        return 0;
    }

    if (E_DB_INSERT_TYPE_IGNORE == insert_mode)
    {
        insert_sql_cmd = SQL_INSERT_IGNORE_TABLE;
    }
    else if (E_DB_INSERT_TYPE_REPLACE == insert_mode)
    {
        insert_sql_cmd = SQL_INSERT_REPLACE_TABLE;
    }
    else
    {
        insert_sql_cmd = SQL_INSERT_TABLE;
    }

    DB_START_TRANSACTION(db_handle);

    for (size_t i = 0; i < item_array_size; ++i)
    {
        sql_ret = sqlite3_prepare_v3(db_handle, insert_sql_cmd, -1, 0, &stmt, &ztail);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_prepare_v3 fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle));
            DB_ROLLBACK_TRANSACTION(db_handle);
            sqlite3_finalize(stmt);
            return -1;
        }

        sqlite3_bind_text(stmt, E_ITEM_IN_1, item_array[i].key, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, E_ITEM_IN_2, item_array[i].type);
        sqlite3_bind_int(stmt, E_ITEM_IN_3, item_array[i].value_len);
        switch (item_array[i].type)
        {
            case E_DB_VALUE_TYPE_BOOL:
                sscanf(item_array[i].value, "%" PRIu64, &int64_value);
                if (int64_value != 0)
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_4, 1);
                }
                else
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_4, 0);
                }
                sqlite3_bind_int64(stmt, E_ITEM_IN_5, 0);
                sqlite3_bind_int64(stmt, E_ITEM_IN_6, 1);
                sscanf(item_array[i].defaults, "%" PRIu64, &int64_value);
                if (int64_value != 0)
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_7, 1);
                }
                else
                {
                    sqlite3_bind_int64(stmt, E_ITEM_IN_7, 0);
                }
                break;
            case E_DB_VALUE_TYPE_INT:
                sscanf(item_array[i].value, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_4, int64_value);
                sscanf(item_array[i].min, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_6, int64_value);
                sscanf(item_array[i].max, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_6, int64_value);
                sscanf(item_array[i].defaults, "%" PRIu64, &int64_value);
                sqlite3_bind_int64(stmt, E_ITEM_IN_7, int64_value);
                break;
            case E_DB_VALUE_TYPE_DOUBLE:
                double_value = atof(item_array[i].value);
                sqlite3_bind_double(stmt, E_ITEM_IN_4, double_value);
                double_value = atof(item_array[i].min);
                sqlite3_bind_double(stmt, E_ITEM_IN_5, double_value);
                double_value = atof(item_array[i].max);
                sqlite3_bind_double(stmt, E_ITEM_IN_6, double_value);
                double_value = atof(item_array[i].defaults);
                sqlite3_bind_double(stmt, E_ITEM_IN_7, double_value);
                break;
            case E_DB_VALUE_TYPE_STRING:
                sqlite3_bind_text(stmt, E_ITEM_IN_4, item_array[i].value, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, E_ITEM_IN_7, item_array[i].defaults, -1, SQLITE_STATIC);
                break;
            case E_DB_VALUE_TYPE_BLOB:
            case E_DB_VALUE_TYPE_INTBLOB:
                sqlite3_bind_blob(stmt, E_ITEM_IN_4, item_array[i].value, (int)strlen(item_array[i].value), SQLITE_STATIC);
                sqlite3_bind_blob(stmt, E_ITEM_IN_7, item_array[i].defaults, (int)strlen(item_array[i].value), SQLITE_STATIC);
                break;
            default:
                LOG_PRINT_ERROR("invalid type[%d]", item_array[i].type);
                DB_ROLLBACK_TRANSACTION(db_handle);
                sqlite3_finalize(stmt);
                return -1;
        }
        sqlite3_bind_int(stmt, E_ITEM_IN_8, item_array[i].security);

        sql_ret = sqlite3_step(stmt);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_step key[%s] fail, ret[%d], errcode[%d]", item_array[i].key, sql_ret, sqlite3_errcode(db_handle));
            DB_ROLLBACK_TRANSACTION(db_handle);
            sqlite3_finalize(stmt);
            return -1;
        }

        LOG_PRINT_INFO("load key[%s] ok!", item_array[i].key);
    }

    DB_END_TRANSACTION(db_handle);
    sqlite3_finalize(stmt);

    return ret;
}

static int32_t db_backup(void)
{
    int32_t ret = 0;
    int32_t sql_ret = 0;
    sqlite3 *db_handle_from = db_handle;
    sqlite3 *db_handle_to = NULL;
    sqlite3_backup *db_handle_backup = NULL;

    do
    {
        sql_ret = sqlite3_open_v2(DB_BACKUP_FILE, &db_handle_to, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        if (SQLITE_OK != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_open_v2 fail, sql_ret[%d]", sql_ret);
            ret = -1;
            break;
        }

        db_handle_backup = sqlite3_backup_init(db_handle_to, "main", db_handle_from, "main");
        if (NULL == db_handle_backup)
        {
            LOG_PRINT_ERROR("sqlite3_backup_init fail, errcode[%d]", sqlite3_errcode(db_handle_to));
            ret = -1;
            break;
        }

        sql_ret = sqlite3_backup_step(db_handle_backup, -1); // -1 表示拷贝全部
        int32_t finish_ret = sqlite3_backup_finish(db_handle_backup);
        if (SQLITE_DONE != sql_ret)
        {
            LOG_PRINT_ERROR("sqlite3_backup_step fail, sql_ret[%d], errcode[%d]", sql_ret, sqlite3_errcode(db_handle_to));
            ret = -1;
            break;
        }

        if (SQLITE_OK != finish_ret)
        {
            LOG_PRINT_WARN("sqlite3_backup_finish fail, finish_ret[%d]", finish_ret);
            ret = -1;
            break;
        }

        ret = 0;
        break;
    } while (0);

    if (NULL != db_handle_to)
    {
        sqlite3_close_v2(db_handle_to);
    }

    return ret;
}

static int32_t db_init(void)
{
    int32_t ret = 0;
    int32_t db_is_exist = 0;
    int32_t db_is_ok = 0;
    int32_t backup_db_is_exist = 0;
    int32_t backup_db_is_ok = 0;

    do
    {
        // db path
        if (0 != access(DB_PATH, F_OK))
        {
            ret = util_execute_command("mkdir -p " DB_PATH);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("mkdir -p fail, ret[%d]", ret);
                break;
            }
        }

        // backup db path
        if (0 != access(DB_BACKUP_PATH, F_OK))
        {
            ret = util_execute_command("mkdir -p " DB_BACKUP_PATH);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("mkdir -p fail, ret[%d]", ret);
                break;
            }
        }

        // db file
        if (0 == access(DB_FILE, F_OK))
        {
            db_is_exist = 1;
            ret = db_integrity_check(DB_FILE);
            if (0 != ret)
            {
                db_is_ok = 0;
            }
            else
            {
                db_is_ok = 1;
            }
        }
        else
        {
            db_is_exist = 0;
        }

        // backup db file
        if (0 == access(DB_BACKUP_FILE, F_OK))
        {
            backup_db_is_exist = 1;
            ret = db_integrity_check(DB_BACKUP_FILE);
            if (0 != ret)
            {
                backup_db_is_ok = 0;
                LOG_PRINT_ERROR("db_integrity_check db backup file fail, ret[%d]!", ret);
            }
            else
            {
                backup_db_is_ok = 1;
            }
        }
        else
        {
            backup_db_is_exist = 0;
        }

        LOG_PRINT_INFO("db exist[%d]-check[%d]; backup db exist[%d]-check[%d]", db_is_exist, db_is_ok, backup_db_is_exist, backup_db_is_ok);
        if (!db_is_exist || !db_is_ok)
        {
            if (!db_is_ok)
            {
                remove(DB_FILE);
            }

            if (backup_db_is_exist && backup_db_is_ok)
            {
                // use backup db
                ret = util_execute_command("cp " DB_BACKUP_FILE " " DB_FILE);
            }
            else
            {
                if (!backup_db_is_ok)
                {
                    remove(DB_BACKUP_FILE);
                }

                ret = db_create(DB_FILE);
                if (0 != ret)
                {
                    break;
                }
            }
        }
        else
        {
            ret = db_open(DB_FILE);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("db_open fail, ret[%d]", ret);
                break;
            }

            ret = db_table_is_exist();
            if (0 != ret)
            {
                LOG_PRINT_ERROR("db_table is not exist, ret[%d]", ret);
                ret = db_create(DB_FILE);
                if (0 != ret)
                {
                    break;
                }
            }
        }

        // ram table -> db(only new item)
        ret = db_load(E_DB_INSERT_TYPE_IGNORE, g_default_db_item_array, g_default_db_item_array_size);
        if (0 != ret)
        {
            LOG_PRINT_ERROR("db_load fail, ret[%d]", ret);
            break;
        }
        db_backup();

        // config -> db(ota update)
        if (0 == access(DB_UPDATE_JSON_FILE, F_OK))
        {
            char db_version[DB_VALUE_LEN] = {};
            int64_t db_version_number = 0;
            size_t db_version_len = sizeof(db_version) - 1;
            ret = db_select("DB_VERSION", E_DB_VALUE_TYPE_INT, db_version, &db_version_len);
            if (0 != ret)
            {
                LOG_PRINT_ERROR("select DB_VERSION fail, then force update!");
                db_version_number = 0;
            }
            else
            {
                sscanf(db_version, "%" PRIu64, &db_version_number);
                LOG_PRINT_INFO("select DB_VERSION[%s]-[%" PRIu64 "]-[%zu]", db_version, db_version_number, db_version_len);
            }

            // json -> item_array;
            // version check;
            int64_t db_ota_version_number = 0;
            db_item_t item_array[128] = {};
            size_t item_array_size = 0;
            // todo

            if (db_ota_version_number >= db_version_number)
            {
                ret = db_load(E_DB_INSERT_TYPE_REPLACE, item_array, item_array_size);
                if (0 != ret)
                {
                    LOG_PRINT_ERROR("db_load fail, ret[%d]", ret);
                    break;
                }
                db_backup();
            }
            remove(DB_UPDATE_JSON_FILE);
        }
        else
        {
            LOG_PRINT_INFO("no need to update from config");
        }

        break;
    } while (0);

    return ret;
}

int main(void)
{
    int32_t ret = 0;
    LOG_PRINT_INFO("sqlite version[%s]-threadsafe[%d]", sqlite3_libversion(), sqlite3_threadsafe());
    ret = db_init();

    return ret;
}
