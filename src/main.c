#include <zephyr/kernel.h>

#include "controller_state.h"

int main(void)
{
    struct controller_snapshot snapshot;

    controller_state_init();
    controller_state_get_snapshot(&snapshot);
    printk("Controller started: actuator=%s health=%s faults=0x%08x\n",
           controller_actuator_state_name(snapshot.actuator_state),
           controller_health_state_name(snapshot.health_state),
           (uint32_t)snapshot.active_faults);

    return 0;
}
