#include <zephyr/kernel.h>
#include "posix_board_if.h"

#include "actuator_state_machine.h"
#include "condition_monitor.h"
#include "diagnostics.h"
#include "fault_injector.h"
#include "monitoring_pipeline.h"
#include "plant_model.h"
#include "safety_supervisor.h"

static struct plant_model model;

static void request_state(enum controller_actuator_state next)
{
    if (actuator_state_machine_request(next) != ACTUATOR_TRANSITION_OK) {
        posix_exit(1);
    }
}

static void emit(const char *event)
{
    struct plant_snapshot normal;
    struct fault_injection_result effective;
    struct controller_snapshot controller;
    struct diagnostics_snapshot diagnostics;

    plant_model_get_snapshot(&model, &normal);
    /* No runtime workers are linked: the injection configuration cannot change
     * between pipeline evaluation and this observation of effective measurements.
     */
    fault_injector_apply(&normal, &effective);
    controller_state_get_snapshot(&controller);
    diagnostics_get_snapshot(&diagnostics);
    printk("%llu,%d,%u,%u,%s,%s,0x%08x,0x%08x,%u,0x%02x,%u,%s,"
           "%d,%d,%u,%u,%u,%u\n",
           (unsigned long long)normal.simulation_time_ms,
           effective.sample.temperature_mdeg_c, effective.sample.vibration_um_s,
           effective.sample.current_ma,
           controller_actuator_state_name(controller.actuator_state),
           controller_health_state_name(controller.health_state),
           controller.active_faults, (unsigned int)fault_injector_get_active(),
           (unsigned int)effective.valid, diagnostics.stale_services,
           (unsigned int)diagnostics.watchdog_fault, event,
           CONDITION_TEMPERATURE_WARNING_MDEG_C, CONDITION_TEMPERATURE_FAULT_MDEG_C,
           CONDITION_VIBRATION_WARNING_UM_S, CONDITION_VIBRATION_FAULT_UM_S,
           CONDITION_CURRENT_WARNING_MA, CONDITION_CURRENT_FAULT_MA);
}

static void cycle(const char *event)
{
    struct controller_snapshot controller;
    struct plant_snapshot normal;

    controller_state_get_snapshot(&controller);
    plant_model_step(&model, controller.actuator_state == CONTROLLER_ACTUATOR_RUNNING);
    plant_model_get_snapshot(&model, &normal);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_PLANT, normal.simulation_time_ms);
    monitoring_pipeline_process(&normal);
    diagnostics_heartbeat_at(DIAGNOSTIC_SERVICE_MONITOR, normal.simulation_time_ms);
    diagnostics_evaluate_at(normal.simulation_time_ms);
    safety_supervisor_evaluate();
    emit(event);
}

int main(void)
{
    controller_state_init();
    fault_injector_init();
    diagnostics_init(0);
    plant_model_init(&model);
    printk("time_ms,temperature_mdeg_c,vibration_um_s,current_ma,actuator,health,"
           "fault_mask,injection_mask,sensor_valid,stale_services,watchdog_fault,event,"
           "temperature_warning_mdeg_c,temperature_fault_mdeg_c,"
           "vibration_warning_um_s,vibration_fault_um_s,current_warning_ma,current_fault_ma\n");
    emit("safe_startup");
    request_state(CONTROLLER_ACTUATOR_STARTING);
    cycle("start_requested");
    request_state(CONTROLLER_ACTUATOR_RUNNING);
    cycle("running");
    for (int i = 0; i < 18; ++i) {
        cycle("");
    }
    fault_injector_enable(FAULT_INJECTION_OVERTEMPERATURE);
    cycle("overtemperature_injected_and_trip");
    for (int i = 0; i < 4; ++i) {
        cycle("");
    }
    fault_injector_clear_all();
    cycle("injection_cleared_health_recovered");
    for (int i = 0; i < 6; ++i) {
        cycle(i == 0 ? "faulted_latch_retained" : "");
    }
    if (actuator_state_machine_reset() != ACTUATOR_TRANSITION_OK) {
        posix_exit(1);
    }
    cycle("explicit_safe_reset");
    for (int i = 0; i < 10; ++i) {
        cycle("");
    }
    posix_exit(0);
    return 0;
}
