/// Avoid include conflicts

#if __has_include("paper/shared/queue/lightweightsemaphore.h")
#include "paper/shared/queue/lightweightsemaphore.h"
#else
#include "../queue_internal/lightweightsemaphore.h"
#endif