#include <zephyr/ztest.h>
#include "controller_state.h"
#include "safety_supervisor.h"

static void assert_supervision(enum controller_actuator_state initial,
                               enum controller_health_state health,
                               enum controller_actuator_state expected)
{
    struct controller_snapshot state;

    controller_state_set_actuator_state(initial);
    controller_state_set_health_state(health);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, expected);
    zassert_equal(state.health_state, health);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}

ZTEST(safety_supervisor, test_healthy_preserves_actuator)
{
    assert_supervision(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_HEALTHY,
                       CONTROLLER_ACTUATOR_RUNNING);
}

ZTEST(safety_supervisor, test_warning_does_not_shutdown)
{
    assert_supervision(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_WARNING,
                       CONTROLLER_ACTUATOR_RUNNING);
}

ZTEST(safety_supervisor, test_fault_stops_running_actuator)
{
    assert_supervision(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_FAULT,
                       CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(safety_supervisor, test_fault_stops_starting_actuator)
{
    assert_supervision(CONTROLLER_ACTUATOR_STARTING, CONTROLLER_HEALTH_FAULT,
                       CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(safety_supervisor, test_emergency_stop_faults_active_actuator)
{
    assert_supervision(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_EMERGENCY_STOP,
                       CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(safety_supervisor, test_recovered_health_does_not_unlatch_faulted)
{
    struct controller_snapshot state;

    assert_supervision(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_FAULT,
                       CONTROLLER_ACTUATOR_FAULTED);
    controller_state_set_health_state(CONTROLLER_HEALTH_HEALTHY);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}
