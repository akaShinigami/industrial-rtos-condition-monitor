#include <zephyr/ztest.h>
#include "actuator_state_machine.h"
#include "condition_monitor.h"
#include "diagnostics.h"
#include "fault_injector.h"
#include "monitoring_pipeline.h"
#include "safety_supervisor.h"
#include "sensor_health.h"

static const struct plant_snapshot normal = {.temperature_mdeg_c = 25000};

static void start_running(void)
{
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_STARTING),
                  ACTUATOR_TRANSITION_OK);
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_RUNNING),
                  ACTUATOR_TRANSITION_OK);
}

ZTEST(batch2_integration, test_process_fault_path)
{
    struct plant_model model;
    struct plant_snapshot sample;
    struct controller_snapshot state;

    plant_model_init(&model);
    plant_model_get_snapshot(&model, &sample);
    start_running();
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    monitoring_pipeline_process(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    plant_model_get_snapshot(&model, &sample);
    zassert_equal(sample.temperature_mdeg_c, 25000);
    zassert_equal(sample.simulation_time_ms, 0U);
}

ZTEST(batch2_integration, test_multiple_process_faults)
{
    struct fault_injection_result effective;
    struct controller_snapshot state;

    fault_injector_enable(FAULT_INJECTION_VIBRATION | FAULT_INJECTION_OVERCURRENT);
    fault_injector_apply(&normal, &effective);
    zassert_equal(effective.sample.vibration_um_s, 4500U);
    zassert_equal(effective.sample.current_ma, 8000U);
    zassert_equal(effective.sample.temperature_mdeg_c, 25000);
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults,
                  CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(batch2_integration, test_sensor_failure_path)
{
    struct controller_snapshot state;

    start_running();
    fault_injector_enable(FAULT_INJECTION_SENSOR_FAILURE);
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SENSOR_FAILURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(batch2_integration, test_watchdog_path)
{
    struct diagnostics_snapshot diagnostic;
    struct controller_snapshot state;

    start_running();
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 501);
    diagnostics_evaluate_at(501);
    diagnostics_get_snapshot(&diagnostic);
    zassert_equal(diagnostic.stale_services, BIT(DIAGNOSTIC_SERVICE_MONITOR));
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(batch2_integration, test_recovery_and_latching)
{
    struct controller_snapshot state;

    start_running();
    fault_injector_enable(FAULT_INJECTION_SENSOR_FAILURE);
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
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

ZTEST(batch2_integration, test_warning_survives_other_owner_updates)
{
    const struct plant_snapshot warning = {.temperature_mdeg_c = 50000};
    struct controller_snapshot state;

    condition_monitor_apply(&warning);
    sensor_health_update(false);
    diagnostics_evaluate_at(501);
    sensor_health_update(true);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 600);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, 600);
    diagnostics_evaluate_at(600);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_WARNING);
    zassert_equal(state.active_faults, 0U);
}

ZTEST(batch2_integration, test_emergency_stop_is_preserved)
{
    struct controller_snapshot state;

    start_running();
    controller_state_set_health_state(CONTROLLER_HEALTH_EMERGENCY_STOP);
    monitoring_pipeline_process(&normal);
    diagnostics_evaluate_at(0);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_EMERGENCY_STOP);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
}

ZTEST(batch2_integration, test_invalid_sample_retains_process_fault)
{
    struct controller_snapshot state;

    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    monitoring_pipeline_process(&normal);
    fault_injector_clear_all();
    fault_injector_enable(FAULT_INJECTION_SENSOR_FAILURE);
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults,
                  CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_SENSOR_FAILURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    fault_injector_clear_all();
    monitoring_pipeline_process(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, 0U);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
}
