#include <zephyr/kernel.h>

#include "controller_state.h"
#include "diagnostics.h"

static struct diagnostics_snapshot state;
K_MUTEX_DEFINE(diagnostics_mutex);

bool diagnostics_heartbeat_is_stale(int64_t now_ms, int64_t last_ms, uint32_t deadline_ms)
{
    if (last_ms < 0 || now_ms < last_ms) {
        return true;
    }
    return (uint64_t)(now_ms - last_ms) > deadline_ms;
}

void diagnostics_init(int64_t now_ms)
{
    k_mutex_lock(&diagnostics_mutex, K_FOREVER);
    state = (struct diagnostics_snapshot){.evaluation_time_ms = now_ms};
    for (int i = 0; i < DIAGNOSTIC_SERVICE_COUNT; ++i) {
        state.services[i].last_heartbeat_ms = now_ms;
        state.services[i].deadline_ms = DIAGNOSTICS_HEARTBEAT_DEADLINE_MS;
    }
    controller_state_update_owned_faults(CONTROLLER_FAULT_SOFTWARE_WATCHDOG, 0U);
    k_mutex_unlock(&diagnostics_mutex);
}

void diagnostics_heartbeat_at(enum diagnostic_service service, int64_t now_ms)
{
    if ((unsigned int)service >= DIAGNOSTIC_SERVICE_COUNT || now_ms < 0) {
        return;
    }
    k_mutex_lock(&diagnostics_mutex, K_FOREVER);
    if (now_ms >= state.services[service].last_heartbeat_ms) {
        state.services[service].last_heartbeat_ms = now_ms;
        state.services[service].heartbeat_received = true;
    }
    k_mutex_unlock(&diagnostics_mutex);
}

void diagnostics_heartbeat(enum diagnostic_service service)
{
    diagnostics_heartbeat_at(service, k_uptime_get());
}

static void evaluate_locked(int64_t now_ms)
{
    state.stale_services = 0U;
    for (int i = 0; i < DIAGNOSTIC_SERVICE_COUNT; ++i) {
        state.services[i].stale = diagnostics_heartbeat_is_stale(
            now_ms, state.services[i].last_heartbeat_ms, state.services[i].deadline_ms);
        if (state.services[i].stale) {
            state.stale_services |= BIT(i);
        }
    }
    state.watchdog_fault = state.stale_services != 0U;
    state.evaluation_time_ms = now_ms;
    /* Keep evaluations serialized through controller publication. Lock order:
     * diagnostics_mutex -> controller state_mutex; no reverse acquisition.
     */
    controller_state_update_owned_faults(CONTROLLER_FAULT_SOFTWARE_WATCHDOG,
        state.watchdog_fault ? CONTROLLER_FAULT_SOFTWARE_WATCHDOG : 0U);
}

void diagnostics_evaluate_at(int64_t now_ms)
{
    k_mutex_lock(&diagnostics_mutex, K_FOREVER);
    evaluate_locked(now_ms);
    k_mutex_unlock(&diagnostics_mutex);
}

void diagnostics_evaluate(void)
{
    k_mutex_lock(&diagnostics_mutex, K_FOREVER);
    /* A heartbeat cannot advance beyond this sampled time while we evaluate. */
    evaluate_locked(k_uptime_get());
    k_mutex_unlock(&diagnostics_mutex);
}

void diagnostics_get_snapshot(struct diagnostics_snapshot *snapshot)
{
    k_mutex_lock(&diagnostics_mutex, K_FOREVER);
    *snapshot = state;
    k_mutex_unlock(&diagnostics_mutex);
}
