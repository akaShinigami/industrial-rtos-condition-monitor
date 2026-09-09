#include <zephyr/kernel.h>

#include "diagnostics.h"
#include "diagnostics_service.h"
#include "safety_supervisor.h"

K_SEM_DEFINE(diagnostics_ready, 0, 1);

static void diagnostics_worker(void)
{
    struct diagnostics_snapshot snapshot;
    uint32_t previous_stale = 0U;

    k_sem_take(&diagnostics_ready, K_FOREVER);
    while (true) {
        diagnostics_evaluate();
        diagnostics_get_snapshot(&snapshot);
        safety_supervisor_evaluate();
        if (snapshot.stale_services != previous_stale) {
            printk("Software watchdog: stale services=0x%02x (plant=bit0 monitor=bit1)\n",
                   snapshot.stale_services);
            previous_stale = snapshot.stale_services;
        }
        k_sleep(K_MSEC(DIAGNOSTICS_SERVICE_PERIOD_MS));
    }
}

/* More scheduling importance than monitoring (4) and plant simulation (5). */
K_THREAD_DEFINE(diagnostics_tid, DIAGNOSTICS_SERVICE_STACK_SIZE, diagnostics_worker,
                NULL, NULL, NULL, DIAGNOSTICS_SERVICE_PRIORITY, 0, 0);

void diagnostics_service_init(void)
{
    k_sem_give(&diagnostics_ready);
}
