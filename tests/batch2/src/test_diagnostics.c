#include <zephyr/ztest.h>
#include "actuator_state_machine.h"
#include "condition_monitor.h"
#include "diagnostics.h"
#include "safety_supervisor.h"
#include "sensor_health.h"

static void refresh_both(int64_t now_ms)
{
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, now_ms);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, now_ms);
    diagnostics_evaluate_at(now_ms);
}

static void start_running(void)
{
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_STARTING),
                  ACTUATOR_TRANSITION_OK);
    zassert_equal(actuator_state_machine_request(CONTROLLER_ACTUATOR_RUNNING),
                  ACTUATOR_TRANSITION_OK);
}

ZTEST(diagnostics, test_initially_healthy)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_get_snapshot(&snapshot);
    zassert_equal(snapshot.stale_services, 0U);
    zassert_false(snapshot.watchdog_fault);
    zassert_equal(snapshot.evaluation_time_ms, 0);
    for (int i = 0; i < DIAGNOSTIC_SERVICE_COUNT; ++i) {
        zassert_equal(snapshot.services[i].last_heartbeat_ms, 0);
        zassert_equal(snapshot.services[i].deadline_ms, 500U);
        zassert_false(snapshot.services[i].stale);
        zassert_false(snapshot.services[i].heartbeat_received);
    }
}

ZTEST(diagnostics, test_fresh_plant)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 100);
    diagnostics_evaluate_at(200);
    diagnostics_get_snapshot(&snapshot);
    zassert_false(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].stale);
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].heartbeat_received);
    zassert_equal(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].last_heartbeat_ms, 100);
    zassert_false(snapshot.watchdog_fault);
}

ZTEST(diagnostics, test_fresh_monitor)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, 100);
    diagnostics_evaluate_at(200);
    diagnostics_get_snapshot(&snapshot);
    zassert_false(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].stale);
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].heartbeat_received);
    zassert_equal(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].last_heartbeat_ms, 100);
    zassert_false(snapshot.watchdog_fault);
}

ZTEST(diagnostics, test_exact_deadline_healthy)
{
    struct diagnostics_snapshot snapshot;

    refresh_both(100);
    diagnostics_evaluate_at(600);
    diagnostics_get_snapshot(&snapshot);
    zassert_equal(snapshot.stale_services, 0U);
    zassert_false(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].stale);
    zassert_false(snapshot.watchdog_fault);
}

ZTEST(diagnostics, test_plant_beyond_deadline)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 100);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, 600);
    diagnostics_evaluate_at(601);
    diagnostics_get_snapshot(&snapshot);
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].stale);
    zassert_false(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].stale);
    zassert_true(snapshot.watchdog_fault);
}

ZTEST(diagnostics, test_monitor_beyond_deadline)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, 100);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 600);
    diagnostics_evaluate_at(601);
    diagnostics_get_snapshot(&snapshot);
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].stale);
    zassert_false(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].stale);
    zassert_true(snapshot.watchdog_fault);
}

ZTEST(diagnostics, test_identifies_one_stale_service)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, 501);
    diagnostics_evaluate_at(501);
    diagnostics_get_snapshot(&snapshot);
    zassert_equal(snapshot.stale_services, BIT(DIAGNOSTIC_SERVICE_MONITOR));
    zassert_equal(snapshot.evaluation_time_ms, 501);
}

ZTEST(diagnostics, test_multiple_stale_services)
{
    struct diagnostics_snapshot snapshot;

    diagnostics_evaluate_at(501);
    diagnostics_get_snapshot(&snapshot);
    zassert_equal(snapshot.stale_services,
                  BIT(DIAGNOSTIC_SERVICE_PLANT) | BIT(DIAGNOSTIC_SERVICE_MONITOR));
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_PLANT].stale);
    zassert_true(snapshot.services[DIAGNOSTIC_SERVICE_MONITOR].stale);
}

ZTEST(diagnostics, test_sets_dedicated_fault)
{
    struct controller_snapshot state;

    diagnostics_evaluate_at(501);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    zassert_false(controller_state_has_faults(CONTROLLER_FAULT_SENSOR_FAILURE));
}

ZTEST(diagnostics, test_watchdog_drives_fault_health)
{
    const struct plant_snapshot normal = {.temperature_mdeg_c = 25000};
    struct controller_snapshot state;

    diagnostics_evaluate_at(501);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    condition_monitor_apply(&normal);
    controller_state_get_snapshot(&state);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
}

ZTEST(diagnostics, test_watchdog_faults_running_actuator)
{
    struct controller_snapshot state;

    start_running();
    diagnostics_evaluate_at(501);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_RUNNING);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
}

ZTEST(diagnostics, test_heartbeat_recovery_clears_fault)
{
    struct controller_snapshot state;
    struct diagnostics_snapshot snapshot;

    diagnostics_evaluate_at(501);
    zassert_true(controller_state_has_faults(CONTROLLER_FAULT_SOFTWARE_WATCHDOG));
    refresh_both(600);
    diagnostics_get_snapshot(&snapshot);
    zassert_false(snapshot.watchdog_fault);
    zassert_equal(snapshot.stale_services, 0U);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, 0U);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
}

ZTEST(diagnostics, test_recovery_preserves_unrelated_faults)
{
    const uint32_t unrelated = CONTROLLER_FAULT_OVERTEMPERATURE |
        CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT |
        CONTROLLER_FAULT_SENSOR_FAILURE;
    struct controller_snapshot state;

    controller_state_set_faults(unrelated);
    diagnostics_evaluate_at(501);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, unrelated | CONTROLLER_FAULT_SOFTWARE_WATCHDOG);
    refresh_both(600);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, unrelated);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(diagnostics, test_recovery_stays_faulted)
{
    struct controller_snapshot state;

    start_running();
    diagnostics_evaluate_at(501);
    safety_supervisor_evaluate();
    refresh_both(600);
    safety_supervisor_evaluate();
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_FAULTED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, 0U);
}

ZTEST(diagnostics, test_reset_requires_all_faults_cleared)
{
    struct controller_snapshot state;

    start_running();
    diagnostics_evaluate_at(501);
    sensor_health_update(false);
    safety_supervisor_evaluate();
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    refresh_both(600);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_RESET_DENIED);
    sensor_health_update(true);
    zassert_equal(actuator_state_machine_reset(), ACTUATOR_TRANSITION_OK);
    controller_state_get_snapshot(&state);
    zassert_equal(state.actuator_state, CONTROLLER_ACTUATOR_STOPPED);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
    zassert_equal(state.active_faults, 0U);
}

ZTEST(diagnostics, test_deterministic_deadline_decision)
{
    zassert_false(diagnostics_heartbeat_is_stale(600, 100, 500));
    zassert_true(diagnostics_heartbeat_is_stale(601, 100, 500));
    zassert_true(diagnostics_heartbeat_is_stale(601, 100, 500));
    zassert_false(diagnostics_heartbeat_is_stale(600, 100, 500));
    zassert_false(diagnostics_heartbeat_is_stale(INT64_MAX, INT64_MAX - 500, 500));
    zassert_true(diagnostics_heartbeat_is_stale(100, 200, 500));
    zassert_true(diagnostics_heartbeat_is_stale(100, -1, 500));
}
