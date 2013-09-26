#include "alloc.h"
#include "buffer.h"
#include "bytestring.h"
#include "frame.h"
#include "frameparser.h"
#include "headerbundle.h"
#include "listener.h"
#include "alloc.h"

#define LIMIT_FRAME_CMD_LINE_LEN      32
#define LIMIT_FRAME_HEADER_LINE_LEN   8192
#define LIMIT_FRAME_HEADER_LINE_COUNT 128
#define LIMIT_FRAME_BODY_LEN          (1024 * 1024 * 10)

