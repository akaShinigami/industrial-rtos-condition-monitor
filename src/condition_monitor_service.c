#include <zephyr/kernel.h>
#include "condition_monitor_service.h"
#include "condition_monitor.h"
#include "plant_simulator.h"
#include "safety_supervisor.h"
static void worker(void){struct plant_snapshot s;while(true){plant_simulator_get_snapshot(&s);condition_monitor_apply(&s);safety_supervisor_evaluate();k_sleep(K_MSEC(100));}}
K_THREAD_DEFINE(condition_monitor_tid,1024,worker,NULL,NULL,NULL,4,0,0);
void condition_monitor_service_init(void){}
