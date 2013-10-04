#include <stdio.h>

#include "alloc.h"
#include "buffer.h"
#include "bytestring.h"
#include "frame.h"
#include "frameparser.h"
#include "frameserializer.h"
#include "headerbundle.h"
#include "connection.h"
#include "connectionbundle.h"
#include "listener.h"
#include "alloc.h"

#define LIMIT_FRAME_CMD_LINE_LEN      32
#define LIMIT_FRAME_HEADER_LINE_LEN   8192
#define LIMIT_FRAME_HEADER_LINE_COUNT 128
#define LIMIT_FRAME_BODY_LEN          (1024 * 1024 * 10)

#define LIMIT_HEARTBEAT_FREQ_MIN      10000  // 10 seconds
#define DEFAULT_HEARTBEAT_FREQ        30000  // 30 seconds

#define NETWORK_READ_SIZE             4096  // Read in 4KiB chunks
