#include <zephyr/ztest.h>
#include "fault_injector.h"
#include "actuator_state_machine.h"
#include "condition_monitor.h"
#include "monitoring_pipeline.h"
#include "sensor_health.h"

ZTEST(fault_injector, test_initially_disabled)
{
    zassert_equal(fault_injector_get_active(), FAULT_INJECTION_NONE);
}

static const struct plant_snapshot normal = {25000, 1500, 4000, 1234};

static void assert_sample_equal(const struct plant_snapshot *a, const struct plant_snapshot *b)
{
    zassert_equal(a->temperature_mdeg_c, b->temperature_mdeg_c);
    zassert_equal(a->vibration_um_s, b->vibration_um_s);
    zassert_equal(a->current_ma, b->current_ma);
    zassert_equal(a->simulation_time_ms, b->simulation_time_ms);
}

ZTEST(fault_injector, test_normal_passes_unchanged)
{
    struct fault_injection_result result;

    fault_injector_apply(&normal, &result);
    assert_sample_equal(&normal, &result.sample);
    zassert_true(result.valid);
}

ZTEST(fault_injector, test_overtemperature)
{
    struct fault_injection_result result;
    struct plant_snapshot expected = normal;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    fault_injector_apply(&normal, &result);
    expected.temperature_mdeg_c = 70000;
    assert_sample_equal(&expected, &result.sample);
    zassert_true(result.sample.temperature_mdeg_c > CONDITION_TEMPERATURE_FAULT_MDEG_C);
    zassert_true(result.valid);
    zassert_equal(normal.temperature_mdeg_c, 25000);
}

ZTEST(fault_injector, test_vibration)
{
    struct fault_injection_result result;
    struct plant_snapshot expected = normal;

    fault_injector_enable(FAULT_INJECTION_VIBRATION);
    fault_injector_apply(&normal, &result);
    expected.vibration_um_s = 4500;
    assert_sample_equal(&expected, &result.sample);
    zassert_true(result.sample.vibration_um_s > CONDITION_VIBRATION_FAULT_UM_S);
    zassert_true(result.valid);
}

ZTEST(fault_injector, test_overcurrent)
{
    struct fault_injection_result result;
    struct plant_snapshot expected = normal;

    fault_injector_enable(FAULT_INJECTION_OVERCURRENT);
    fault_injector_apply(&normal, &result);
    expected.current_ma = 8000;
    assert_sample_equal(&expected, &result.sample);
    zassert_true(result.sample.current_ma > CONDITION_CURRENT_FAULT_MA);
    zassert_true(result.valid);
}

ZTEST(fault_injector, test_simultaneous_injections)
{
    struct fault_injection_result result;
    const struct plant_snapshot expected = {70000, 4500, 8000, 1234};

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE | FAULT_INJECTION_VIBRATION |
                          FAULT_INJECTION_OVERCURRENT);
    fault_injector_apply(&normal, &result);
    assert_sample_equal(&expected, &result.sample);
    zassert_true(result.valid);
}

ZTEST(fault_injector, test_disable_preserves_others)
{
    struct fault_injection_result result;
    struct plant_snapshot expected = normal;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE | FAULT_INJECTION_VIBRATION);
    fault_injector_disable(FAULT_INJECTION_OVERTEMPERATURE);
    zassert_equal(fault_injector_get_active(), FAULT_INJECTION_VIBRATION);
    fault_injector_apply(&normal, &result);
    expected.vibration_um_s = 4500;
    assert_sample_equal(&expected, &result.sample);
}

ZTEST(fault_injector, test_clear_all)
{
    struct fault_injection_result result;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE | FAULT_INJECTION_VIBRATION |
                          FAULT_INJECTION_OVERCURRENT | FAULT_INJECTION_SENSOR_FAILURE);
    fault_injector_clear_all();
    zassert_equal(fault_injector_get_active(), FAULT_INJECTION_NONE);
    fault_injector_apply(&normal, &result);
    assert_sample_equal(&normal, &result.sample);
    zassert_true(result.valid);
}

ZTEST(fault_injector, test_repeated_apply)
{
    struct fault_injection_result first;
    struct fault_injection_result second;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE | FAULT_INJECTION_SENSOR_FAILURE);
    fault_injector_apply(&normal, &first);
    fault_injector_apply(&normal, &second);
    assert_sample_equal(&first.sample, &second.sample);
    zassert_equal(first.valid, second.valid);
}

ZTEST(fault_injector, test_sensor_failure_is_explicit)
{
    struct fault_injection_result result;

    fault_injector_enable(FAULT_INJECTION_SENSOR_FAILURE);
    fault_injector_apply(&normal, &result);
    zassert_false(result.valid);
    assert_sample_equal(&normal, &result.sample);
}

ZTEST(fault_injector, test_sensor_failure_sets_only_owned_flag)
{
    struct controller_snapshot state;

    sensor_health_update(false);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SENSOR_FAILURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    /* A normal process assessment must not hide a sensor failure. */
    condition_monitor_apply(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SENSOR_FAILURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(fault_injector, test_sensor_recovery_preserves_unrelated_faults)
{
    const uint32_t unrelated = CONTROLLER_FAULT_OVERTEMPERATURE |
        CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT |
        CONTROLLER_FAULT_SOFTWARE_WATCHDOG;
    struct controller_snapshot state;

    controller_state_set_faults(unrelated);
    sensor_health_update(false);
    sensor_health_update(true);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, unrelated);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(fault_injector, test_temperature_flows_through_monitor)
{
    struct fault_injection_result result;
    struct controller_snapshot state;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    fault_injector_apply(&normal, &result);
    condition_monitor_apply(&result.sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(fault_injector, test_multiple_process_flags)
{
    struct fault_injection_result result;
    struct controller_snapshot state;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE | FAULT_INJECTION_VIBRATION);
    fault_injector_apply(&normal, &result);
    condition_monitor_apply(&result.sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults,
                  CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(fault_injector, test_process_recovery_remains_latched)
{
    struct controller_snapshot state;

    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_STARTING),
                  ACTUATOR_TRANSITION_OK);
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_RUNNING),
                  ACTUATOR_TRANSITION_OK);
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    fault_injector_clear_all();
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, 0U);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_OK);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
}
