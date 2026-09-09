#include <zephyr/kernel.h>

#include "controller_state.h"
#include "plant_simulator.h"
#include "condition_monitor_service.h"
#include "diagnostics.h"
#include "diagnostics_service.h"
#include "fault_injector.h"

int main(void)
{
    struct controller_snapshot controller_snapshot;
    struct plant_snapshot plant_snapshot;
    struct diagnostics_snapshot diagnostics_snapshot;

    controller_state_init();
    fault_injector_init();
    diagnostics_init(k_uptime_get());
    plant_simulator_init();
    condition_monitor_service_init();
    diagnostics_service_init();
    diagnostics_get_snapshot(&diagnostics_snapshot);
    printk("Software watchdog: %s, deadline=%u ms; injections=0x%x\n",
           diagnostics_snapshot.watchdog_fault ? "FAULT" : "HEALTHY",
           DIAGNOSTICS_HEARTBEAT_DEADLINE_MS, (unsigned int)fault_injector_get_active());

    controller_state_get_snapshot(&controller_snapshot);
    printk("Controller started: actuator=%s health=%s faults=0x%08x\n",
           controller_actuator_state_name(controller_snapshot.actuator_state),
           controller_health_state_name(controller_snapshot.health_state),
           (uint32_t)controller_snapshot.active_faults);

    plant_simulator_get_snapshot(&plant_snapshot);
    printk("Plant: temp=%d.%03d C vibration=%u.%03u mm/s current=%u.%03u A t=%lld ms\n",
           plant_snapshot.temperature_mdeg_c / 1000,
           plant_snapshot.temperature_mdeg_c % 1000,
           plant_snapshot.vibration_um_s / 1000,
           plant_snapshot.vibration_um_s % 1000,
           plant_snapshot.current_ma / 1000,
           plant_snapshot.current_ma % 1000,
           (long long)plant_snapshot.simulation_time_ms);

    return 0;
}
