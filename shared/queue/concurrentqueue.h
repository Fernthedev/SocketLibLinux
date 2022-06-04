/// Avoid include conflicts

#if __has_include("paper/shared/queue/concurrentqueue.h")
#include "paper/shared/queue/concurrentqueue.h"
#else
#include "../queue/concurrentqueue.h"
#endif