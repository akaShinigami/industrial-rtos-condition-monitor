#include <zephyr/ztest.h>

#include "controller_state.h"

static void reset_controller(void *fixture)
{
    ARG_UNUSED(fixture);
    controller_state_init();
}

ZTEST_SUITE(controller_state, NULL, NULL, reset_controller, NULL, NULL);

ZTEST(controller_state, test_initial_state_is_safe)
{
    struct controller_snapshot snapshot;
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    zassert_equal(snapshot.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(snapshot.active_faults, CONTROLLER_FAULT_NONE);
    zassert_true(snapshot.timestamp_ms >= 0);
}

ZTEST(controller_state, test_actuator_state_update)
{
    struct controller_snapshot snapshot;
    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_RUNNING);
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.actuator_state, CONTROLLER_ACTUATOR_RUNNING);
}

ZTEST(controller_state, test_health_state_update)
{
    struct controller_snapshot snapshot;
    controller_state_set_health_state(CONTROLLER_HEALTH_WARNING);
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.health_state, CONTROLLER_HEALTH_WARNING);
}

ZTEST(controller_state, test_setting_one_fault)
{
    controller_state_set_faults(CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_true(controller_state_has_faults(CONTROLLER_FAULT_OVERTEMPERATURE));
}

ZTEST(controller_state, test_setting_multiple_faults)
{
    uint32_t expected = CONTROLLER_FAULT_OVERTEMPERATURE |
                        CONTROLLER_FAULT_EXCESSIVE_VIBRATION;
    struct controller_snapshot snapshot;
    controller_state_set_faults(CONTROLLER_FAULT_OVERTEMPERATURE);
    controller_state_set_faults(CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.active_faults, expected);
    zassert_true(controller_state_has_faults(expected));
}

ZTEST(controller_state, test_clearing_one_fault_preserves_others)
{
    controller_state_set_faults(CONTROLLER_FAULT_OVERTEMPERATURE |
                                CONTROLLER_FAULT_OVERCURRENT);
    controller_state_clear_faults(CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_false(controller_state_has_faults(CONTROLLER_FAULT_OVERTEMPERATURE));
    zassert_true(controller_state_has_faults(CONTROLLER_FAULT_OVERCURRENT));
}

ZTEST(controller_state, test_clearing_all_faults)
{
    controller_state_set_faults(CONTROLLER_FAULT_OVERTEMPERATURE |
                                CONTROLLER_FAULT_SENSOR_FAILURE);
    controller_state_clear_all_faults();
    zassert_false(controller_state_has_faults(CONTROLLER_FAULT_OVERTEMPERATURE));
    zassert_false(controller_state_has_faults(CONTROLLER_FAULT_SENSOR_FAILURE));
}

ZTEST(controller_state, test_snapshot_is_consistent)
{
    struct controller_snapshot snapshot;
    uint32_t faults = CONTROLLER_FAULT_OVERCURRENT |
                      CONTROLLER_FAULT_SENSOR_FAILURE;
    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    controller_state_set_health_state(CONTROLLER_HEALTH_FAULT);
    controller_state_set_faults(faults);
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(snapshot.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(snapshot.active_faults, faults);
}

ZTEST(controller_state, test_state_names)
{
    zassert_str_equal(controller_actuator_state_name(CONTROLLER_ACTUATOR_STOPPED), "STOPPED");
    zassert_str_equal(controller_health_state_name(CONTROLLER_HEALTH_EMERGENCY_STOP),
                      "EMERGENCY_STOP");
    zassert_str_equal(controller_actuator_state_name((enum controller_actuator_state)99),
                      "UNKNOWN");
    zassert_str_equal(controller_health_state_name((enum controller_health_state)-1), "UNKNOWN");
}

ZTEST(controller_state, test_checked_actuator_update_preserves_health_and_faults)
{
    struct controller_snapshot snapshot;

    controller_state_set_health_state(CONTROLLER_HEALTH_WARNING);
    controller_state_set_faults(CONTROLLER_FAULT_SENSOR_FAILURE);
    zassert_true(controller_state_compare_exchange_actuator(CONTROLLER_ACTUATOR_STOPPED,
                                                            CONTROLLER_ACTUATOR_STARTING));
    controller_state_get_snapshot(&snapshot);
    zassert_equal(snapshot.actuator_state, CONTROLLER_ACTUATOR_STARTING);
    zassert_equal(snapshot.health_state, CONTROLLER_HEALTH_WARNING);
    zassert_equal(snapshot.active_faults, CONTROLLER_FAULT_SENSOR_FAILURE);
}

ZTEST(controller_state, test_checked_update_rejects_state_changed_since_snapshot)
{
    struct controller_snapshot before;
    struct controller_snapshot after;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_STARTING);
    controller_state_get_snapshot(&before);
    /* Deterministically represent a safety trip after the caller's read. */
    controller_state_set_faults(CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    controller_state_set_health_state(CONTROLLER_HEALTH_FAULT);
    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    zassert_false(controller_state_compare_exchange_actuator(before.actuator_state,
                                                             CONTROLLER_ACTUATOR_RUNNING));
    controller_state_get_snapshot(&after);
    zassert_equal(after.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(after.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(after.active_faults, CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
}

ZTEST(controller_state, test_reset_checks_fault_added_after_healthy_observation)
{
    struct controller_snapshot before;
    struct controller_snapshot after;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    controller_state_get_snapshot(&before);
    zassert_equal(before.active_faults, CONTROLLER_FAULT_NONE);
    zassert_equal(before.health_state, CONTROLLER_HEALTH_HEALTHY);
    controller_state_set_faults(CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_false(controller_state_try_reset_actuator());
    controller_state_get_snapshot(&after);
    zassert_equal(after.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(after.active_faults, CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_equal(after.health_state, CONTROLLER_HEALTH_HEALTHY);
}

ZTEST(controller_state, test_reset_checks_health_changed_after_healthy_observation)
{
    struct controller_snapshot before;
    struct controller_snapshot after;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    controller_state_get_snapshot(&before);
    zassert_equal(before.health_state, CONTROLLER_HEALTH_HEALTHY);
    controller_state_set_health_state(CONTROLLER_HEALTH_EMERGENCY_STOP);
    zassert_false(controller_state_try_reset_actuator());
    controller_state_get_snapshot(&after);
    zassert_equal(after.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(after.health_state, CONTROLLER_HEALTH_EMERGENCY_STOP);
    zassert_equal(after.active_faults, CONTROLLER_FAULT_NONE);
}
