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
/** Set the overall system health state. */
void controller_state_set_health_state(enum controller_health_state state);
/** Set one or more fault flags without clearing currently active flags. */
void controller_state_set_faults(uint32_t faults);
/** Clear one or more fault flags without affecting other active flags. */
void controller_state_clear_faults(uint32_t faults);
/** Clear every active fault flag. */
void controller_state_clear_all_faults(void);
/** Return whether every requested fault flag is active. */
bool controller_state_has_faults(uint32_t faults);
/** Convert an actuator state to a name, or "UNKNOWN" if invalid. */
const char *controller_actuator_state_name(enum controller_actuator_state state);
/** Convert a health state to a name, or "UNKNOWN" if invalid. */
const char *controller_health_state_name(enum controller_health_state state);

#endif /* CONTROLLER_STATE_H_ */
