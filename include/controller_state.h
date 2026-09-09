#ifndef CONTROLLER_STATE_H_
#define CONTROLLER_STATE_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>

/** Operating state of the monitored actuator. */
enum controller_actuator_state {
    CONTROLLER_ACTUATOR_STOPPED,
    CONTROLLER_ACTUATOR_STARTING,
    CONTROLLER_ACTUATOR_RUNNING,
    CONTROLLER_ACTUATOR_STOPPING,
    CONTROLLER_ACTUATOR_FAULTED,
};

/** Overall condition of the controller system. */
enum controller_health_state {
    CONTROLLER_HEALTH_HEALTHY,
    CONTROLLER_HEALTH_WARNING,
    CONTROLLER_HEALTH_FAULT,
    CONTROLLER_HEALTH_EMERGENCY_STOP,
};

/** Fault flags; multiple flags may be active at once. */
enum controller_fault_flag {
    CONTROLLER_FAULT_NONE = 0U,
    CONTROLLER_FAULT_OVERTEMPERATURE = BIT(0),
    CONTROLLER_FAULT_EXCESSIVE_VIBRATION = BIT(1),
    CONTROLLER_FAULT_OVERCURRENT = BIT(2),
    CONTROLLER_FAULT_SENSOR_FAILURE = BIT(3),
    CONTROLLER_FAULT_SOFTWARE_WATCHDOG = BIT(4),
};

/** Atomically observed controller state for diagnostics. */
struct controller_snapshot {
    enum controller_actuator_state actuator_state;
    enum controller_health_state health_state;
    uint32_t active_faults;
    int64_t timestamp_ms;
};

/** Reset controller state to STOPPED, HEALTHY, and no active faults. */
void controller_state_init(void);
/** Copy a consistent controller-state snapshot into @p snapshot. */
void controller_state_get_snapshot(struct controller_snapshot *snapshot);
/** Set the actuator operating state. */
void controller_state_set_actuator_state(enum controller_actuator_state state);
/**
 * Commit an actuator change only if its current state still equals expected.
 * The state machine validates the expected/desired transition before calling;
 * comparison and mutation share one lock, so a stale decision cannot overwrite
 * an intervening actuator change. Does not change health or faults.
 */
bool controller_state_compare_exchange_actuator(enum controller_actuator_state expected,
                                               enum controller_actuator_state desired);
/**
 * Atomically reset FAULTED to STOPPED only with no faults and health below FAULT.
 * Validation and mutation use the same lock as health/fault updates.
 */
bool controller_state_try_reset_actuator(void);
/** Set the overall system health state. */
void controller_state_set_health_state(enum controller_health_state state);
/** Set one or more fault flags without clearing currently active flags. */
void controller_state_set_faults(uint32_t faults);
/** Clear one or more fault flags without affecting other active flags. */
void controller_state_clear_faults(uint32_t faults);
/** Clear every active fault flag. */
void controller_state_clear_all_faults(void);
/**
 * Atomically replace only owned fault bits and recompute aggregate health.
 * All active bits are critical. Process warning is retained across other owners'
 * updates; EMERGENCY_STOP is preserved until explicitly changed by its caller.
 * Runtime owners use these APIs instead of separate fault/health setters.
 */
void controller_state_update_owned_faults(uint32_t owned, uint32_t active);
/** Replace the three process fault bits and the process warning contribution. */
void controller_state_update_process(uint32_t active, bool warning);
/** Return whether every requested fault flag is active. */
bool controller_state_has_faults(uint32_t faults);
/** Convert an actuator state to a name, or "UNKNOWN" if invalid. */
const char *controller_actuator_state_name(enum controller_actuator_state state);
/** Convert a health state to a name, or "UNKNOWN" if invalid. */
const char *controller_health_state_name(enum controller_health_state state);

#endif /* CONTROLLER_STATE_H_ */
