#include "controller_state.h"
#include "sensor_health.h"

void sensor_health_update(bool valid)
{
    controller_state_update_owned_faults(CONTROLLER_FAULT_SENSOR_FAILURE,
                                        valid ? 0U : CONTROLLER_FAULT_SENSOR_FAILURE);
}
