#pragma once
#include <mutex>
#include "esp_err.h"
// Pinned ESP-Hosted 1.4.0 internal entry point, declared in transport_drv.h.
// Its public reset API aborts on failure instead of returning the error.
extern "C" esp_err_t transport_drv_reconfigure(void);
namespace Pine {
// ESP-Hosted 1.4.0 reconfiguration has shared, unsynchronized reset/retry state.
// Serialize the first Wi-Fi and NimBLE controller initialization on the C6.
inline std::mutex& tab5RadioInitMutex(){static std::mutex mutex;return mutex;}
}
