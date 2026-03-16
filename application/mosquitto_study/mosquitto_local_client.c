#include "utility/utils.h"
#include "mosquitto.h"

// sudo apt-get install -y mosquitto

typedef struct local_client_userdata_t
{
    uint32_t status; // 0 default; 1 connected; 2 disconnected;
    bool mult;
    bool reconnect;

    int qos0_pub_mid;
    int qos1_pub_mid;
    int qos2_pub_mid;

    // single
    int qos0_sub_mid;
    int qos1_sub_mid;
    int qos2_sub_mid;
    int qos0_unsub_mid;
    int qos1_unsub_mid;
    int qos2_unsub_mid;

    // mult
    int mult_sub_mid;
    int mult_unsub_mid;
} local_client_userdata_t;

static local_client_userdata_t g_local_client_userdata = {
    .status = 0,
    .mult = true,
    .reconnect = true,
    0,
};

#define MESSAGE0 "message0"
#define MESSAGE1 "message1"
#define MESSAGE2 "message2"

char *subs[] = {"qos2/test0", "qos2/test1", "qos2/test2"};
char *unsubs[] = {"qos2/test0", "qos2/test1", "qos2/test2"};

#define MOSQUITTO_CLIENT_NUMBER     (1)
#define MOSQUITTO_CLIENT_ID         "local_client_" UTIL_TOSTR(MOSQUITTO_CLIENT_NUMBER)
#define MOSQUITTO_WILL_TOPIC_ALIVE  "local_client_alive_" UTIL_TOSTR(MOSQUITTO_CLIENT_NUMBER)
#define MOSQUITTO_HOST              "localhost"
#define MOSQUITTO_PORT              (1883)
#define MOSQUITTO_KEEPALIVE_SECONDS (60)

void on_connect_v5(struct mosquitto *mosq, void *obj, int rc, int flags, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(props);
    local_client_userdata_t *p_userdata = (local_client_userdata_t *)obj;
    LOG_PRINT_INFO("on_connect_v5 ret[%d], flags[%d]", rc, flags);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        return;
    }
    else
    {
        p_userdata->status = 1;
        if (p_userdata->mult)
        {
            mosquitto_subscribe_multiple(mosq, &p_userdata->mult_sub_mid, UTIL_ARRAY_SIZE(subs), subs, 2, 0, NULL);
        }
        else
        {
            mosquitto_subscribe(mosq, &p_userdata->qos0_sub_mid, subs[0], 2);
            mosquitto_subscribe(mosq, &p_userdata->qos1_sub_mid, subs[1], 2);
            mosquitto_subscribe(mosq, &p_userdata->qos2_sub_mid, subs[2], 2);
        }
    }
}

void on_disconnect_v5(struct mosquitto *mosq, void *obj, int rc, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(props);
    local_client_userdata_t *p_userdata = (local_client_userdata_t *)obj;
    LOG_PRINT_INFO("on_disconnect_v5 ret[%d]", rc);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        return;
    }
    p_userdata->status = 2;
}

void on_subscribe_v5(struct mosquitto *mosq, void *obj, int mid, int sub_count, const int *granted_qos, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(props);
    local_client_userdata_t *p_userdata = (local_client_userdata_t *)obj;
    LOG_PRINT_INFO("on_subscribe_v5 mid[%d]-sub_count[%d]", mid, sub_count);
    for (int i = 0; i < sub_count; ++i)
    {
        LOG_PRINT_INFO("on_subscribe_v5 granted_qos[%d]", granted_qos[i]);
    }

    if (p_userdata->mult)
    {
        if (mid == p_userdata->mult_sub_mid)
        {
            mosquitto_publish_v5(mosq, &p_userdata->qos0_pub_mid, subs[0], strlen(MESSAGE0), MESSAGE0, 2, false, NULL);
            mosquitto_publish_v5(mosq, &p_userdata->qos1_pub_mid, subs[1], strlen(MESSAGE1), MESSAGE1, 2, false, NULL);
            mosquitto_publish_v5(mosq, &p_userdata->qos2_pub_mid, subs[2], strlen(MESSAGE2), MESSAGE2, 2, false, NULL);
        }
    }
    else
    {
        if (mid == p_userdata->qos0_sub_mid)
        {
            mosquitto_publish_v5(mosq, &p_userdata->qos0_pub_mid, subs[0], strlen(MESSAGE0), MESSAGE0, 2, false, NULL);
        }
        else if (mid == p_userdata->qos1_sub_mid)
        {
            mosquitto_publish_v5(mosq, &p_userdata->qos1_pub_mid, subs[1], strlen(MESSAGE1), MESSAGE1, 2, false, NULL);
        }
        else if (mid == p_userdata->qos2_sub_mid)
        {
            mosquitto_publish_v5(mosq, &p_userdata->qos2_pub_mid, subs[2], strlen(MESSAGE2), MESSAGE2, 2, false, NULL);
        }
    }
}

void on_unsubscribe_v5(struct mosquitto *mosq, void *obj, int mid, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(props);
    local_client_userdata_t *p_userdata = (local_client_userdata_t *)obj;
    LOG_PRINT_INFO("on_unsubscribe_v5 mid[%d]", mid);

    if (p_userdata->mult)
    {
        if (mid == p_userdata->mult_unsub_mid)
        {
            mosquitto_disconnect(mosq);
        }
    }
    else
    {
        if (mid == p_userdata->qos2_unsub_mid)
        {
            mosquitto_disconnect(mosq);
        }
    }
}

void on_publish_v5(struct mosquitto *mosq, void *obj, int mid, int reason_code, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(reason_code);
    UTIL_UNUSED(obj);
    UTIL_UNUSED(props);
    LOG_PRINT_INFO("on_publish_v5 mid[%d]-reason_code[%d][%s]", mid, reason_code, mosquitto_reason_string(reason_code));
}

void on_message_v5(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg, const mosquitto_property *props)
{
    UTIL_UNUSED(mosq);
    local_client_userdata_t *p_userdata = (local_client_userdata_t *)obj;
    UTIL_UNUSED(props);
    LOG_PRINT_INFO("on_message_v5 mid[%d]-topic[%s]-payload_len[%d]-qos[%d]-retain[%d]",
                   msg->mid,
                   msg->topic,
                   msg->payloadlen,
                   msg->qos,
                   msg->retain);

    if (p_userdata->mult)
    {
        if (msg->mid == p_userdata->mult_sub_mid)
        {
            mosquitto_unsubscribe_multiple(mosq, &p_userdata->mult_unsub_mid, UTIL_ARRAY_SIZE(unsubs), unsubs, NULL);
        }
    }
    else
    {
        if (msg->mid == p_userdata->qos0_sub_mid)
        {
            mosquitto_unsubscribe(mosq, &p_userdata->qos0_unsub_mid, unsubs[0]);
        }
        else if (msg->mid == p_userdata->qos1_sub_mid)
        {
            mosquitto_unsubscribe(mosq, &p_userdata->qos1_unsub_mid, unsubs[1]);
        }
        else if (msg->mid == p_userdata->qos2_sub_mid)
        {
            mosquitto_unsubscribe(mosq, &p_userdata->qos2_unsub_mid, unsubs[2]);
        }
    }
}

static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    UTIL_UNUSED(mosq);
    UTIL_UNUSED(obj);
    switch (level)
    {
        case MOSQ_LOG_INFO:
            LOG_PRINT_INFO("%s", str);
            break;
        case MOSQ_LOG_WARNING:
            LOG_PRINT_WARN("%s", str);
            break;
        case MOSQ_LOG_ERR:
            LOG_PRINT_ERROR("%s", str);
            break;
        case MOSQ_LOG_DEBUG:
            LOG_PRINT_DEBUG("%s", str);
            break;
        case MOSQ_LOG_NOTICE:
            LOG_PRINT_INFO("notice-%s", str);
            break;
        case MOSQ_LOG_SUBSCRIBE:
            LOG_PRINT_INFO("subscribe-%s", str);
            break;
        case MOSQ_LOG_UNSUBSCRIBE:
            LOG_PRINT_INFO("unsubscribe-%s", str);
            break;
        case MOSQ_LOG_WEBSOCKETS:
            LOG_PRINT_INFO("websockets-%s", str);
            break;
        default:
            break;
    }
}

int main(void)
{
    int rc = MOSQ_ERR_SUCCESS;
    int major = 0, minor = 0, revision = 0;
    struct mosquitto *mosq = NULL;
    mosquitto_lib_version(&major, &minor, &revision);
    LOG_PRINT_INFO("mosquitto version[%d-%d-%d]", major, minor, revision);
    mosquitto_lib_init();

    mosq = mosquitto_new(MOSQUITTO_CLIENT_ID, true, &g_local_client_userdata); // TODO true/false
    if (mosq == NULL)
    {
        LOG_PRINT_ERROR("mosquitto_new fail");
        return -1;
    }
    // mosquitto_log_callback_set(mosq, on_log);
    mosquitto_int_option(mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
    mosquitto_will_set_v5(mosq, MOSQUITTO_WILL_TOPIC_ALIVE, 0, NULL, 1, false, NULL); // TODO true/false
    // mosquitto_username_pw_set(mosq, "uname", ";'[08gn=#");
    mosquitto_connect_v5_callback_set(mosq, on_connect_v5);
    mosquitto_disconnect_v5_callback_set(mosq, on_disconnect_v5);
    mosquitto_subscribe_v5_callback_set(mosq, on_subscribe_v5);
    mosquitto_unsubscribe_v5_callback_set(mosq, on_unsubscribe_v5);
    mosquitto_publish_v5_callback_set(mosq, on_publish_v5);
    mosquitto_message_v5_callback_set(mosq, on_message_v5);
    // mosquitto_message_v5_retry_set(mosq, 20); // default 20

    rc = mosquitto_connect_async(mosq, MOSQUITTO_HOST, MOSQUITTO_PORT, MOSQUITTO_KEEPALIVE_SECONDS);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        LOG_PRINT_ERROR("mosquitto_connect_async fail, ret[%d](%s)", rc, mosquitto_strerror(rc));
        return rc;
    }

#if 0
    while (g_local_client_userdata.status != 2)
    {
        rc = mosquitto_loop(mosq, -1, 1);
        if (rc != MOSQ_ERR_SUCCESS)
        {
            LOG_PRINT_ERROR("mosquitto_connect_async fail, ret[%d](%s)", rc, mosquitto_strerror(rc));
            return rc;
        }
    }
#else
    rc = mosquitto_loop_start(mosq);
    if (0 != rc)
    {
        printf("mosquitto_loop_start fail, ret[%d](%s)", rc, mosquitto_strerror(rc));
        return rc;
    }

    /* 50 millis to be system polite */
    struct timespec tv = {0, 50e6};
    while (g_local_client_userdata.status != 2)
    {
        nanosleep(&tv, NULL);
    }

    mosquitto_loop_stop(mosq, false);
#endif

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}