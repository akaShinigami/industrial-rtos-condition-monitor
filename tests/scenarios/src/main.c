#include <zephyr/ztest.h>

#include "actuator_state_machine.h"
#include "diagnostics.h"
#include "fault_injector.h"
#include "monitoring_pipeline.h"
#include "plant_model.h"
#include "safety_supervisor.h"

static struct plant_model model;

static void initialize_stack(void *fixture)
{
    ARG_UNUSED(fixture);
    controller_state_init();
    fault_injector_init();
    diagnostics_init(0);
    plant_model_init(&model);
}

ZTEST_SUITE(scenarios, NULL, NULL, initialize_stack, NULL, NULL);

ZTEST(scenarios, test_safe_startup)
{
    struct controller_snapshot controller;
    struct diagnostics_snapshot diagnostics;
    struct plant_snapshot plant;

    controller_state_get_snapshot(&controller);
    diagnostics_get_snapshot(&diagnostics);
    plant_model_get_snapshot(&model, &plant);
    zassert_equal(controller.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    zassert_equal(controller.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(controller.active_faults, 0U);
    zassert_equal(fault_injector_get_active(), FAULT_INJECTION_NONE);
    zassert_false(diagnostics.watchdog_fault);
    zassert_equal(diagnostics.stale_services, 0U);
    zassert_equal(diagnostics.evaluation_time_ms, 0);
    zassert_equal(plant.temperature_mdeg_c, 25000);
    zassert_equal(plant.vibration_um_s, 0U);
    zassert_equal(plant.current_ma, 0U);
    zassert_equal(plant.simulation_time_ms, 0U);
}

static void request_state(enum controller_actuator_state next)
{
    zassert_equal(actuator_state_machine_request(next), ACTUATOR_TRANSITION_OK);
}

static void start_running(void)
{
    request_state(CONTROLLER_ACTUATOR_STARTING);
    request_state(CONTROLLER_ACTUATOR_RUNNING);
}

/* Drive the same modules as the runtime workers, using plant time as uptime.
 * Suppression models a missed heartbeat, never a frozen Zephyr thread.
 */
static void cycle(bool plant_heartbeat, bool monitor_heartbeat)
{
    struct controller_snapshot controller;
    struct plant_snapshot plant;

    controller_state_get_snapshot(&controller);
    plant_model_step(&model, controller.actuator_state == CONTROLLER_ACTUATOR_RUNNING);
    plant_model_get_snapshot(&model, &plant);
    if (plant_heartbeat) {
        diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, plant.simulation_time_ms);
    }
    monitoring_pipeline_process(&plant);
    if (monitor_heartbeat) {
        diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, plant.simulation_time_ms);
    }
    diagnostics_evaluate_at(plant.simulation_time_ms);
    safety_supervisor_evaluate();
}

static void assert_controller(enum controller_actuator_state actuator,
                              enum controller_health_state health, uint32_t faults)
{
    struct controller_snapshot controller;

    controller_state_get_snapshot(&controller);
    zassert_equal(controller.actuator_state, actuator);
    zassert_equal(controller.health_state, health);
    zassert_equal(controller.active_faults, faults);
}

static void reset_safely(void)
{
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_OK);
    assert_controller(CONTROLLER_ACTUATOR_STOPPED, CONTROLLER_HEALTH_HEALTHY, 0U);
}

ZTEST(scenarios, test_nominal_start_run_stop)
{
    struct plant_snapshot plant;
    int32_t previous_temperature = 25000;

    request_state(CONTROLLER_ACTUATOR_STARTING);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_STARTING, CONTROLLER_HEALTH_HEALTHY, 0U);
    request_state(CONTROLLER_ACTUATOR_RUNNING);
    for (int i = 0; i < 10; ++i) {
        cycle(true, true);
        plant_model_get_snapshot(&model, &plant);
        zassert_equal(plant.current_ma, PLANT_MODEL_RUNNING_CURRENT_MA);
        zassert_equal(plant.vibration_um_s, PLANT_MODEL_RUNNING_VIBRATION_UM_S);
        zassert_true(plant.temperature_mdeg_c > previous_temperature);
        zassert_true(plant.temperature_mdeg_c < PLANT_MODEL_RUNNING_TEMPERATURE_MDEG_C);
        previous_temperature = plant.temperature_mdeg_c;
        assert_controller(CONTROLLER_ACTUATOR_RUNNING, CONTROLLER_HEALTH_HEALTHY, 0U);
    }
    request_state(CONTROLLER_ACTUATOR_STOPPING);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_STOPPING, CONTROLLER_HEALTH_HEALTHY, 0U);
    request_state(CONTROLLER_ACTUATOR_STOPPED);
    cycle(true, true);
    plant_model_get_snapshot(&model, &plant);
    zassert_equal(plant.current_ma, 0U);
    zassert_equal(plant.vibration_um_s, 0U);
    zassert_true(plant.temperature_mdeg_c < previous_temperature);
    zassert_true(plant.temperature_mdeg_c >= 25000);
    assert_controller(CONTROLLER_ACTUATOR_STOPPED, CONTROLLER_HEALTH_HEALTHY, 0U);
}

ZTEST(scenarios, test_overtemperature_trip_recovery_reset)
{
    start_running();
    cycle(true, true);
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    fault_injector_clear_all();
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    reset_safely();
}

ZTEST(scenarios, test_multiple_process_faults_recover_independently)
{
    start_running();
    cycle(true, true);
    fault_injector_enable(FAULT_INJECTION_VIBRATION | FAULT_INJECTION_OVERCURRENT);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT);
    fault_injector_disable(FAULT_INJECTION_VIBRATION);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERCURRENT);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    fault_injector_disable(FAULT_INJECTION_OVERCURRENT);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    reset_safely();
}

ZTEST(scenarios, test_sensor_failure_recovery_preserves_watchdog)
{
    struct plant_snapshot plant;
    struct fault_injection_result effective;

    start_running();
    cycle(true, true);
    fault_injector_enable(FAULT_INJECTION_SENSOR_FAILURE);
    plant_model_get_snapshot(&model, &plant);
    fault_injector_apply(&plant, &effective);
    zassert_false(effective.valid);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SENSOR_FAILURE);
    for (int i = 0; i < 6; ++i) {
        cycle(true, false);
    }
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SENSOR_FAILURE | CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    fault_injector_clear_all();
    cycle(true, false);
    plant_model_get_snapshot(&model, &plant);
    fault_injector_apply(&plant, &effective);
    zassert_true(effective.valid);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    reset_safely();
}

ZTEST(scenarios, test_watchdog_recovery_preserves_process_fault)
{
    struct diagnostics_snapshot diagnostics;

    start_running();
    cycle(true, true);
    for (int i = 0; i < 6; ++i) {
        cycle(true, false);
    }
    diagnostics_get_snapshot(&diagnostics);
    zassert_equal(diagnostics.stale_services, BIT(DIAGNOSTIC_SERVICE_MONITOR));
    zassert_true(diagnostics.watchdog_fault);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    fault_injector_enable(FAULT_INJECTION_OVERCURRENT);
    cycle(true, false);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SOFTWARE_WATCHDOG | CONTROLLER_FAULT_OVERCURRENT);
    cycle(true, true);
    diagnostics_get_snapshot(&diagnostics);
    zassert_false(diagnostics.watchdog_fault);
    zassert_equal(diagnostics.stale_services, 0U);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERCURRENT);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    fault_injector_clear_all();
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    reset_safely();
}

ZTEST(scenarios, test_mixed_ownership_process_recovers_first)
{
    start_running();
    cycle(true, true);
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    for (int i = 0; i < 6; ++i) {
        cycle(true, false);
    }
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    fault_injector_clear_all();
    cycle(true, false);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    cycle(true, true);
    assert_controller(CONTROLLER_ACTUATOR_FAULTED, CONTROLLER_HEALTH_HEALTHY, 0U);
    reset_safely();
}

struct replay_checkpoint {
    struct fault_injection_result effective;
    struct controller_snapshot controller;
    struct diagnostics_snapshot diagnostics;
    enum fault_injection_flag injections;
};

static void capture(struct replay_checkpoint *checkpoint)
{
    struct plant_snapshot plant;

    plant_model_get_snapshot(&model, &plant);
    fault_injector_apply(&plant, &checkpoint->effective);
    controller_state_get_snapshot(&checkpoint->controller);
    diagnostics_get_snapshot(&checkpoint->diagnostics);
    checkpoint->injections = fault_injector_get_active();
}

static void replay(struct replay_checkpoint checkpoints[8])
{
    initialize_stack(NULL);
    capture(&checkpoints[0]);
    start_running();
    for (int i = 0; i < 5; ++i) {
        cycle(true, true);
    }
    capture(&checkpoints[1]);
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    cycle(true, true);
    capture(&checkpoints[2]);
    for (int i = 0; i < 6; ++i) {
        cycle(true, false);
    }
    capture(&checkpoints[3]);
    fault_injector_clear_all();
    cycle(true, false);
    capture(&checkpoints[4]);
    cycle(true, true);
    capture(&checkpoints[5]);
    reset_safely();
    capture(&checkpoints[6]);
    cycle(true, true);
    capture(&checkpoints[7]);
}

ZTEST(scenarios, test_deterministic_replay)
{
    static struct replay_checkpoint first[8];
    static struct replay_checkpoint second[8];

    replay(first);
    replay(second);
    zassert_equal(first[1].controller.actuator_state, CONTROLLER_ACTUATOR_RUNNING);
    zassert_equal(first[3].controller.active_faults,
                  CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_equal(first[5].controller.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(first[5].controller.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(first[7].controller.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    for (int i = 0; i < 8; ++i) {
        const struct replay_checkpoint *a = &first[i];
        const struct replay_checkpoint *b = &second[i];

        zassert_equal(a->effective.sample.temperature_mdeg_c, b->effective.sample.temperature_mdeg_c);
        zassert_equal(a->effective.sample.vibration_um_s, b->effective.sample.vibration_um_s);
        zassert_equal(a->effective.sample.current_ma, b->effective.sample.current_ma);
        zassert_equal(a->effective.sample.simulation_time_ms, b->effective.sample.simulation_time_ms);
        zassert_equal(a->effective.valid, b->effective.valid);
        zassert_equal(a->injections, b->injections);
        zassert_equal(a->controller.actuator_state, b->controller.actuator_state);
        zassert_equal(a->controller.health_state, b->controller.health_state);
        zassert_equal(a->controller.active_faults, b->controller.active_faults);
        /* Controller timestamp uses real uptime, not the scenario's clock. */
        zassert_equal(a->diagnostics.evaluation_time_ms, b->diagnostics.evaluation_time_ms);
        zassert_equal(a->diagnostics.stale_services, b->diagnostics.stale_services);
        zassert_equal(a->diagnostics.watchdog_fault, b->diagnostics.watchdog_fault);
        for (int j = 0; j < DIAGNOSTIC_SERVICE_COUNT; ++j) {
            zassert_equal(a->diagnostics.services[j].last_heartbeat_ms,
                          b->diagnostics.services[j].last_heartbeat_ms);
            zassert_equal(a->diagnostics.services[j].deadline_ms,
                          b->diagnostics.services[j].deadline_ms);
            zassert_equal(a->diagnostics.services[j].heartbeat_received,
                          b->diagnostics.services[j].heartbeat_received);
            zassert_equal(a->diagnostics.services[j].stale, b->diagnostics.services[j].stale);
        }
    }
}
