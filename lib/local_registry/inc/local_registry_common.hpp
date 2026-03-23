#pragma once

#include <functional>
#include <memory>
#include <string>
#include <list>
#include <unordered_set>

#include <cstring>
#include <inttypes.h>

#include "hv/hobjectpool.h"
#include "UdsServer.hpp"
#include "UdsClient.hpp"

#include "utility/utils.h"
#include "utility/debug_backtrace.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "local_registry_module.pb.h"

#include "Singleton.hpp"
#include "ThreadQueue.hpp"

#ifndef LOCAL_REGISTRY_MSG_SIZE_MAX
#define LOCAL_REGISTRY_MSG_SIZE_MAX (8 * 1024) // 8 K
#endif

const constexpr std::size_t LOCAL_REGISTRY_CLIENT_HV_LOOP_NUM_MAX = 1;
const constexpr std::size_t LOCAL_REGISTRY_MSG_HEADER_SIZE = sizeof(st_local_msg_header); // client_id, msg_seqid, msg_type, service_id, msgdata_len

// service_id[4 bytes] = module_id[high 2 bytes] + msg_id[low 2 bytes];
#define MODULE_ID_MAX          (UINT16_MAX)
#define EACH_MODULE_MSG_ID_MAX (UINT16_MAX)

#define LOCAL_REGISTRY_SOCKET_LEN_MAX   (108)
#define LOCAL_REGISTRY_CLIENT_NAME_MAX  (32)
#define SOCKET_PATH_PREFIX              "/tmp/"
#define LOCAL_REGISTRY_SOCKET_FILE      "local_registry.ipc"
#define LOCAL_REGISTRY_SOCKET_PATH      SOCKET_PATH_PREFIX LOCAL_REGISTRY_SOCKET_FILE
#define LOCAL_REGISTRY_CTRL_SOCKET_FILE "local_registry_ctrl.ipc"
#define LOCAL_REGISTRY_CTRL_SOCKET_PATH SOCKET_PATH_PREFIX LOCAL_REGISTRY_CTRL_SOCKET_FILE

#define LOCAL_REGISTEY_SOCKET_FMT  SOCKET_PATH_PREFIX "%u-%s.ipc"       // communication with daemon     : client_pid-client_name.ipc
#define LOCAL_REGISTEY_SOCKET_FMT1 SOCKET_PATH_PREFIX "%u-listen-1.ipc" // listen                        : client_id-listen-1.ipc
#define LOCAL_REGISTEY_SOCKET_FMT2 SOCKET_PATH_PREFIX "%u-%s-%u-2.ipc"  // communication with each other : client_src_id-client_name-client_target_id-2.ipc

#define LOCAL_REGISTRY_CLIENT_ONCE_COUNT_MAX                  (128) // client count max
#define LOCAL_REGISTRY_CLIENT_PROVIDE_SERVICES_ONCE_COUNT_MAX (128) // one client can provider max 128 services
#define LOCAL_REGISTRY_CLIENT_CONSUME_SERVICES_ONCE_COUNT_MAX (128) // one client can consumer max 128 services

#define LOCAL_REGISTRY_COMMUNICATION_TIMEOUT_MS (1500)

// Internal state constants
#define IPC_HV_SOA_COND_STATE_INIT         (-1)
#define IPC_HV_SOA_COND_STATE_SUCCESS      (0)
#define IPC_HV_SOA_COND_STATE_CONNECTED    (1) // mean connected
#define IPC_HV_SOA_COND_STATE_DISCONNECTED (2) // mean disconnected

// Repeat constants for timer
#define IPC_HV_SOA_TIMER_ID_INVALID     (0)
#define IPC_HV_SOA_TIMER_REPEAT_INVALID (0)
#define IPC_HV_SOA_TIMER_REPEAT_ONCE    (1)
#define IPC_HV_SOA_TIMER_REPEAT_CYCLE   (UINT32_MAX)
