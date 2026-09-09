#include <zephyr/kernel.h>

#include "controller_state.h"
#include "plant_simulator.h"
#include "condition_monitor_service.h"

int main(void)
{
    struct controller_snapshot controller_snapshot;
    struct plant_snapshot plant_snapshot;

    controller_state_init();
    plant_simulator_init();
    condition_monitor_service_init();
    /* The monitor worker starts automatically and keeps STOPPED plant healthy. */

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
