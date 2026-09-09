#include <zephyr/ztest.h>
#include "condition_monitor.h"

static void assert_assessment(struct plant_snapshot sample,
                              enum condition_level temperature,
                              enum condition_level vibration,
                              enum condition_level current,
                              enum controller_health_state health,
                              uint32_t faults)
{
    struct condition_assessment assessment;

    condition_monitor_evaluate(&sample, &assessment);
    zassert_equal(assessment.temperature, temperature);
    zassert_equal(assessment.vibration, vibration);
    zassert_equal(assessment.current, current);
    zassert_equal(assessment.health, health);
    zassert_equal(assessment.fault_flags, faults);
}

ZTEST(condition_monitor, test_normal_measurements_are_healthy)
{
    assert_assessment((struct plant_snapshot){25000, 1500, 4000, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_HEALTHY, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_temperature_below_warning)
{
    assert_assessment((struct plant_snapshot){49999, 0, 0, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_HEALTHY, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_temperature_at_warning)
{
    assert_assessment((struct plant_snapshot){50000, 0, 0, 0},
                      CONDITION_WARNING, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_WARNING, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_temperature_at_fault)
{
    assert_assessment((struct plant_snapshot){60000, 0, 0, 0},
                      CONDITION_FAULT, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_OVERTEMPERATURE);
}

ZTEST(condition_monitor, test_vibration_below_warning)
{
    assert_assessment((struct plant_snapshot){25000, 2499, 0, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_HEALTHY, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_vibration_at_warning)
{
    assert_assessment((struct plant_snapshot){25000, 2500, 0, 0},
                      CONDITION_NORMAL, CONDITION_WARNING, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_WARNING, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_vibration_at_fault)
{
    assert_assessment((struct plant_snapshot){25000, 3500, 0, 0},
                      CONDITION_NORMAL, CONDITION_FAULT, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
}

ZTEST(condition_monitor, test_current_below_warning)
{
    assert_assessment((struct plant_snapshot){25000, 0, 5499, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_HEALTHY, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_current_at_warning)
{
    assert_assessment((struct plant_snapshot){25000, 0, 5500, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_WARNING,
                      CONTROLLER_HEALTH_WARNING, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_current_at_fault)
{
    assert_assessment((struct plant_snapshot){25000, 0, 7000, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_FAULT,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_OVERCURRENT);
}

ZTEST(condition_monitor, test_simultaneous_warnings)
{
    assert_assessment((struct plant_snapshot){50000, 2500, 5500, 0},
                      CONDITION_WARNING, CONDITION_WARNING, CONDITION_WARNING,
                      CONTROLLER_HEALTH_WARNING, CONTROLLER_FAULT_NONE);
}

ZTEST(condition_monitor, test_fault_dominates_warning)
{
    assert_assessment((struct plant_snapshot){50000, 3500, 0, 0},
                      CONDITION_WARNING, CONDITION_FAULT, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
}

ZTEST(condition_monitor, test_temperature_and_vibration_faults_compose)
{
    assert_assessment((struct plant_snapshot){60000, 3500, 0, 0},
                      CONDITION_FAULT, CONDITION_FAULT, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
}

ZTEST(condition_monitor, test_all_monitoring_faults_compose)
{
    assert_assessment((struct plant_snapshot){60000, 3500, 7000, 0},
                      CONDITION_FAULT, CONDITION_FAULT, CONDITION_FAULT,
                      CONTROLLER_HEALTH_FAULT,
                      CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_EXCESSIVE_VIBRATION |
                      CONTROLLER_FAULT_OVERCURRENT);
}

ZTEST(condition_monitor, test_temperature_recovery_clears_fault)
{
    struct plant_snapshot sample = {.temperature_mdeg_c = 60000};
    struct controller_snapshot state;

    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_OVERTEMPERATURE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
    sample.temperature_mdeg_c = 25000;
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_NONE);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_HEALTHY);
}

ZTEST(condition_monitor, test_clearing_one_fault_preserves_another)
{
    struct plant_snapshot sample = {60000, 3500, 0, 0};
    struct controller_snapshot state;

    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults,
                  CONTROLLER_FAULT_OVERTEMPERATURE | CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
    sample.temperature_mdeg_c = 25000;
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
    zassert_equal(state.health_state, CONTROLLER_HEALTH_FAULT);
}

ZTEST(condition_monitor, test_clearing_owned_faults_preserves_sensor_failure)
{
    struct plant_snapshot sample = {60000, 3500, 7000, 0};
    struct controller_snapshot state;

    controller_state_set_faults(CONTROLLER_FAULT_SENSOR_FAILURE);
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults,
                  CONTROLLER_FAULT_SENSOR_FAILURE | CONTROLLER_FAULT_OVERTEMPERATURE |
                  CONTROLLER_FAULT_EXCESSIVE_VIBRATION | CONTROLLER_FAULT_OVERCURRENT);
    sample = (struct plant_snapshot){.temperature_mdeg_c = 25000};
    condition_monitor_apply(&sample);
    controller_state_get_snapshot(&state);
    zassert_equal(state.active_faults, CONTROLLER_FAULT_SENSOR_FAILURE);
}

ZTEST(condition_monitor, test_repeated_evaluation_is_identical)
{
    const struct plant_snapshot sample = {50000, 3500, 7000, 1234};
    struct condition_assessment first;
    struct condition_assessment second;

    condition_monitor_evaluate(&sample, &first);
    condition_monitor_evaluate(&sample, &second);
    /* Compare fields, not structure padding. */
    zassert_equal(first.temperature, second.temperature);
    zassert_equal(first.vibration, second.vibration);
    zassert_equal(first.current, second.current);
    zassert_equal(first.health, second.health);
    zassert_equal(first.fault_flags, second.fault_flags);
}

ZTEST(condition_monitor, test_vibration_above_signed_range_is_fault)
{
    assert_assessment((struct plant_snapshot){25000, (uint32_t)INT32_MAX + 1U, 0, 0},
                      CONDITION_NORMAL, CONDITION_FAULT, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
}

ZTEST(condition_monitor, test_maximum_vibration_is_fault)
{
    assert_assessment((struct plant_snapshot){25000, UINT32_MAX, 0, 0},
                      CONDITION_NORMAL, CONDITION_FAULT, CONDITION_NORMAL,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_EXCESSIVE_VIBRATION);
}

ZTEST(condition_monitor, test_current_above_signed_range_is_fault)
{
    assert_assessment((struct plant_snapshot){25000, 0, (uint32_t)INT32_MAX + 1U, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_FAULT,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_OVERCURRENT);
}

ZTEST(condition_monitor, test_maximum_current_is_fault)
{
    assert_assessment((struct plant_snapshot){25000, 0, UINT32_MAX, 0},
                      CONDITION_NORMAL, CONDITION_NORMAL, CONDITION_FAULT,
                      CONTROLLER_HEALTH_FAULT, CONTROLLER_FAULT_OVERCURRENT);
}
