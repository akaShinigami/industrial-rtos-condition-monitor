#include <zephyr/ztest.h>
#include "actuator_state_machine.h"

/* Direct state setup isolates each transition from the other transitions. */
static void assert_transition(enum controller_actuator_state from,
                              enum controller_actuator_state to,
                              enum actuator_transition_result expected)
{
    struct controller_snapshot state;

    controller_state_set_actuator_state(from);
    zassert_equal(actuator_state_machine_request(to), expected);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, expected == ACTUATOR_TRANSITION_OK ? to : from);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}

ZTEST(actuator_state_machine, test_stopped_to_starting)
{
    assert_transition(CONTROLLER_ACTUATOR_STOPPED, CONTROLLER_ACTUATOR_STARTING,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_starting_to_running)
{
    assert_transition(CONTROLLER_ACTUATOR_STARTING, CONTROLLER_ACTUATOR_RUNNING,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_running_to_stopping)
{
    assert_transition(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_ACTUATOR_STOPPING,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_stopping_to_stopped)
{
    assert_transition(CONTROLLER_ACTUATOR_STOPPING, CONTROLLER_ACTUATOR_STOPPED,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_starting_to_faulted)
{
    assert_transition(CONTROLLER_ACTUATOR_STARTING, CONTROLLER_ACTUATOR_FAULTED,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_running_to_faulted)
{
    assert_transition(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_ACTUATOR_FAULTED,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_stopping_to_faulted)
{
    assert_transition(CONTROLLER_ACTUATOR_STOPPING, CONTROLLER_ACTUATOR_FAULTED,
                      ACTUATOR_TRANSITION_OK);
}

ZTEST(actuator_state_machine, test_stopped_to_running_rejected)
{
    assert_transition(CONTROLLER_ACTUATOR_STOPPED, CONTROLLER_ACTUATOR_RUNNING,
                      ACTUATOR_TRANSITION_INVALID);
}

ZTEST(actuator_state_machine, test_running_to_starting_rejected)
{
    assert_transition(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_ACTUATOR_STARTING,
                      ACTUATOR_TRANSITION_INVALID);
}

ZTEST(actuator_state_machine, test_rejected_transition_preserves_state)
{
    struct controller_snapshot before;
    struct controller_snapshot after;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_STARTING);
    controller_state_set_health_state(CONTROLLER_HEALTH_WARNING);
    controller_state_set_faults(CONTROLLER_FAULT_SENSOR_FAILURE);
    controller_state_get_snapshot(&before);
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_STOPPED),
                  ACTUATOR_TRANSITION_INVALID);
    controller_state_get_snapshot(&after);
    zassert_equal(after.actuator_state, before.actuator_state);
    zassert_equal(after.health_state, before.health_state);
    zassert_equal(after.active_faults, before.active_faults);
}

ZTEST(actuator_state_machine, test_faulted_to_running_rejected)
{
    assert_transition(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_ACTUATOR_RUNNING,
                      ACTUATOR_TRANSITION_INVALID);
}

ZTEST(actuator_state_machine, test_reset_denied_with_critical_fault)
{
    struct controller_snapshot state;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    controller_state_set_faults(CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
}

ZTEST(actuator_state_machine, test_reset_denied_with_fault_health)
{
    struct controller_snapshot state;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    controller_state_set_health_state(CONTROLLER_HEALTH_FAULT);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}

ZTEST(actuator_state_machine, test_safe_reset_to_stopped)
{
    struct controller_snapshot state;

    controller_state_set_actuator_state(CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_OK);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
}
