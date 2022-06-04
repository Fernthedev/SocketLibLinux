/// Avoid include conflicts

#if __has_include("paper/shared/queue/blockingconcurrentqueue.h")
#include "paper/shared/queue/blockingconcurrentqueue.h"
#else
#include "../queue/blockingconcurrentqueue.h"
#endif