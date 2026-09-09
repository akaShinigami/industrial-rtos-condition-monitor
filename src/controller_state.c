#include <zephyr/kernel.h>

#include "controller_state.h"

struct controller_state_data {
    enum controller_actuator_state actuator_state;
    enum controller_health_state health_state;
    uint32_t active_faults;
    bool process_warning;
};

static struct controller_state_data state;
K_MUTEX_DEFINE(state_mutex);

void controller_state_init(void)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.actuator_state = CONTROLLER_ACTUATOR_STOPPED;
    state.health_state = CONTROLLER_HEALTH_HEALTHY;
    state.active_faults = CONTROLLER_FAULT_NONE;
    state.process_warning = false;
    k_mutex_unlock(&state_mutex);
}

void controller_state_get_snapshot(struct controller_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    k_mutex_lock(&state_mutex, K_FOREVER);
    snapshot->actuator_state = state.actuator_state;
    snapshot->health_state = state.health_state;
    snapshot->active_faults = state.active_faults;
    snapshot->timestamp_ms = k_uptime_get();
    k_mutex_unlock(&state_mutex);
}

void controller_state_set_actuator_state(enum controller_actuator_state actuator_state)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.actuator_state = actuator_state;
    k_mutex_unlock(&state_mutex);
}

bool controller_state_compare_exchange_actuator(enum controller_actuator_state expected,
                                               enum controller_actuator_state desired)
{
    bool changed = false;

    k_mutex_lock(&state_mutex, K_FOREVER);
    if (state.actuator_state == expected) {
        state.actuator_state = desired;
        changed = true;
    }
    k_mutex_unlock(&state_mutex);
    return changed;
}

bool controller_state_try_reset_actuator(void)
{
    bool reset = false;

    k_mutex_lock(&state_mutex, K_FOREVER);
    if (state.actuator_state == CONTROLLER_ACTUATOR_FAULTED &&
        state.active_faults == CONTROLLER_FAULT_NONE &&
        state.health_state < CONTROLLER_HEALTH_FAULT) {
        state.actuator_state = CONTROLLER_ACTUATOR_STOPPED;
        reset = true;
    }
    k_mutex_unlock(&state_mutex);
    return reset;
}

void controller_state_set_health_state(enum controller_health_state health_state)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.health_state = health_state;
    k_mutex_unlock(&state_mutex);
}

void controller_state_set_faults(uint32_t faults)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.active_faults |= faults;
    k_mutex_unlock(&state_mutex);
}

void controller_state_clear_faults(uint32_t faults)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.active_faults &= ~faults;
    k_mutex_unlock(&state_mutex);
}

void controller_state_clear_all_faults(void)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.active_faults = CONTROLLER_FAULT_NONE;
    k_mutex_unlock(&state_mutex);
}

bool controller_state_has_faults(uint32_t faults)
{
    bool present;

    k_mutex_lock(&state_mutex, K_FOREVER);
    present = (state.active_faults & faults) == faults;
    k_mutex_unlock(&state_mutex);
    return present;
}

const char *controller_actuator_state_name(enum controller_actuator_state actuator_state)
{
    switch (actuator_state) {
    case CONTROLLER_ACTUATOR_STOPPED: return "STOPPED";
    case CONTROLLER_ACTUATOR_STARTING: return "STARTING";
    case CONTROLLER_ACTUATOR_RUNNING: return "RUNNING";
    case CONTROLLER_ACTUATOR_STOPPING: return "STOPPING";
    case CONTROLLER_ACTUATOR_FAULTED: return "FAULTED";
    default: return "UNKNOWN";
    }
}

const char *controller_health_state_name(enum controller_health_state health_state)
{
    switch (health_state) {
    case CONTROLLER_HEALTH_HEALTHY: return "HEALTHY";
    case CONTROLLER_HEALTH_WARNING: return "WARNING";
    case CONTROLLER_HEALTH_FAULT: return "FAULT";
    case CONTROLLER_HEALTH_EMERGENCY_STOP: return "EMERGENCY_STOP";
    default: return "UNKNOWN";
    }
}

/* Caller holds state_mutex. No owner's update can hide another owner's fault. */
static void recompute_health(void)
{
    if (state.health_state != CONTROLLER_HEALTH_EMERGENCY_STOP) {
        state.health_state = state.active_faults ? CONTROLLER_HEALTH_FAULT :
            state.process_warning ? CONTROLLER_HEALTH_WARNING : CONTROLLER_HEALTH_HEALTHY;
    }
}

void controller_state_update_owned_faults(uint32_t owned, uint32_t active)
{
    k_mutex_lock(&state_mutex, K_FOREVER);
    state.active_faults = (state.active_faults & ~owned) | (active & owned);
    recompute_health();
    k_mutex_unlock(&state_mutex);
}

void controller_state_update_process(uint32_t active, bool warning)
{
    const uint32_t owned = CONTROLLER_FAULT_OVERTEMPERATURE |
        CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT;

    k_mutex_lock(&state_mutex, K_FOREVER);
    state.active_faults = (state.active_faults & ~owned) | (active & owned);
    state.process_warning = warning;
    recompute_health();
    k_mutex_unlock(&state_mutex);
}
