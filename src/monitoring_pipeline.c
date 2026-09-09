#include "condition_monitor.h"
#include "fault_injector.h"
#include "monitoring_pipeline.h"
#include "safety_supervisor.h"
#include "sensor_health.h"

void monitoring_pipeline_process(const struct plant_snapshot *normal)
{
    struct fault_injection_result effective;

    fault_injector_apply(normal, &effective);
    /* Invalid data must not clear the last known process faults/warnings. */
    if (effective.valid) {
        condition_monitor_apply(&effective.sample);
    }
    sensor_health_update(effective.valid);
    safety_supervisor_evaluate();
}
