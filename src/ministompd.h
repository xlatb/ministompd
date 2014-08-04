#include <stdio.h>
#include <stdint.h>

typedef uint64_t queue_local_id;  // The id of a specific frame within a queue

#include "alloc.h"
#include "log.h"
#include "buffer.h"
#include "bytestring.h"
#include "hash.h"
#include "bytestring_list.h"
#include "frame.h"
#include "frameparser.h"
#include "frameserializer.h"
#include "headerbundle.h"
#include "connection.h"
#include "connectionbundle.h"
#include "listener.h"
#include "queueconfig.h"
#include "storage.h"
#include "queue.h"
#include "subscription.h"
#include "framerouter.h"

#define LIMIT_FRAME_CMD_LINE_LEN      32
#define LIMIT_FRAME_HEADER_LINE_LEN   8192
#define LIMIT_FRAME_HEADER_LINE_COUNT 128
#define LIMIT_FRAME_BODY_LEN          (1024 * 1024 * 10)

#define LIMIT_HEARTBEAT_FREQ_MIN      10000  // 10 seconds
#define DEFAULT_HEARTBEAT_FREQ        30000  // 30 seconds

#define DEFAULT_QUEUE_SIZE_MAX        1024   // 1024 frames
#define DEFAULT_QUEUE_NACK_MAX        20     // 20 nacks

#define NETWORK_READ_SIZE             4096   // Read in 4KiB chunks
