#include <zephyr/kernel.h>

#include "condition_monitor_service.h"
#include "diagnostics.h"
#include "monitoring_pipeline.h"
#include "plant_simulator.h"

#define CONDITION_MONITOR_PERIOD_MS 100U
#define CONDITION_MONITOR_STACK_SIZE 1024
#define CONDITION_MONITOR_PRIORITY 4

K_SEM_DEFINE(monitor_ready, 0, 1);

static void worker(void)
{
    struct plant_snapshot sample;

    k_sem_take(&monitor_ready, K_FOREVER);
    while (true) {
        plant_simulator_get_snapshot(&sample);
        monitoring_pipeline_process(&sample);
        diagnostics_heartbeat(DIAGNOSTIC_SERVICE_MONITOR);
        k_sleep(K_MSEC(CONDITION_MONITOR_PERIOD_MS));
    }
}

K_THREAD_DEFINE(condition_monitor_tid, CONDITION_MONITOR_STACK_SIZE, worker,
                NULL, NULL, NULL, CONDITION_MONITOR_PRIORITY, 0, 0);

void condition_monitor_service_init(void)
{
    k_sem_give(&monitor_ready);
}
