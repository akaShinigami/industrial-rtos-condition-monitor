#ifndef DIAGNOSTICS_H_
#define DIAGNOSTICS_H_

#include <stdbool.h>
#include <stdint.h>

enum diagnostic_service {
    DIAGNOSTIC_SERVICE_PLANT,
    DIAGNOSTIC_SERVICE_MONITOR,
    DIAGNOSTIC_SERVICE_COUNT,
};

/* Five 100 ms cycles of tolerance; equality is healthy, age > 500 ms is stale. */
#define DIAGNOSTICS_HEARTBEAT_DEADLINE_MS 500U

struct diagnostic_service_snapshot {
    /* Initialization time is the grace-period baseline until the first heartbeat. */
    int64_t last_heartbeat_ms;
    uint32_t deadline_ms;
    bool heartbeat_received;
    bool stale;
};

struct diagnostics_snapshot {
    struct diagnostic_service_snapshot services[DIAGNOSTIC_SERVICE_COUNT];
    /* BIT(enum diagnostic_service) identifies each stale service. */
    uint32_t stale_services;
    bool watchdog_fault;
    int64_t evaluation_time_ms;
};

/** Initialize once before workers; now_ms is nonnegative Zephyr uptime. */
void diagnostics_init(int64_t now_ms);
/** Pure decision function. Invalid/negative/reversed timestamps are stale. */
bool diagnostics_heartbeat_is_stale(int64_t now_ms, int64_t last_ms, uint32_t deadline_ms);
/** Runtime heartbeat uses Zephyr uptime. Call after successful cycle completion. */
void diagnostics_heartbeat(enum diagnostic_service service);
/** Deterministic hook; invalid IDs and negative/out-of-order heartbeats are ignored. */
void diagnostics_heartbeat_at(enum diagnostic_service service, int64_t now_ms);
/** Runtime evaluation samples uptime while holding the diagnostics mutex. */
void diagnostics_evaluate(void);
/** Evaluate at supplied uptime and atomically publish only the watchdog-owned bit. */
void diagnostics_evaluate_at(int64_t now_ms);
/** Copy private state under its mutex; stale fields reflect the last evaluation. */
void diagnostics_get_snapshot(struct diagnostics_snapshot *snapshot);

#endif /* DIAGNOSTICS_H_ */
