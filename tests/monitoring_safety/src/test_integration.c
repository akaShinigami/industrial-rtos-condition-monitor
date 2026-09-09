#include <zephyr/ztest.h>
#include "actuator_state_machine.h"
#include "condition_monitor.h"
#include "safety_supervisor.h"

static void start_actuator(void)
{
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_STARTING),
                  ACTUATOR_TRANSITION_OK);
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_RUNNING),
                  ACTUATOR_TRANSITION_OK);
}

ZTEST(integration, test_critical_measurement_faults_actuator)
{
    const struct plant_snapshot sample = {25000, 0, 7000, 0};
    struct controller_snapshot state;

    start_actuator();
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERCURRENT);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_RUNNING);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERCURRENT);
}

ZTEST(integration, test_simultaneous_critical_measurements_compose)
{
    const struct plant_snapshot sample = {60000, 3500, 7000, 0};
    const uint32_t expected_faults = CONTROLLER_FAULT_OVERTEMPERATURE |
                                     CONTROLLER_FAULT_EXCESSIVE_VIBRATION |
                                     CONTROLLER_FAULT_OVERCURRENT;
    struct controller_snapshot state;

    start_actuator();
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, expected_faults);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, expected_faults);
}

ZTEST(integration, test_recovery_requires_explicit_safe_reset)
{
    struct plant_snapshot sample = {.temperature_mdeg_c = 60000};
    struct controller_snapshot state;

    start_actuator();
    condition_monitor_apply(&sample);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);

    sample.temperature_mdeg_c = 25000;
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);

    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_OK);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}
