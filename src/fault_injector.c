#include <zephyr/kernel.h>

#include "fault_injector.h"

static uint32_t active_flags;
K_MUTEX_DEFINE(injection_mutex);

static const uint32_t supported_flags = FAULT_INJECTION_OVERTEMPERATURE |
    FAULT_INJECTION_VIBRATION | FAULT_INJECTION_OVERCURRENT | FAULT_INJECTION_SENSOR_FAILURE;

void fault_injector_init(void)
{
    fault_injector_clear_all();
}

void fault_injector_enable(enum fault_injection_flag flags)
{
    k_mutex_lock(&injection_mutex, K_FOREVER);
    active_flags |= (uint32_t)flags & supported_flags;
    k_mutex_unlock(&injection_mutex);
}

void fault_injector_disable(enum fault_injection_flag flags)
{
    k_mutex_lock(&injection_mutex, K_FOREVER);
    active_flags &= ~(uint32_t)flags;
    k_mutex_unlock(&injection_mutex);
}

void fault_injector_clear_all(void)
{
    k_mutex_lock(&injection_mutex, K_FOREVER);
    active_flags = FAULT_INJECTION_NONE;
    k_mutex_unlock(&injection_mutex);
}

enum fault_injection_flag fault_injector_get_active(void)
{
    uint32_t flags;

    k_mutex_lock(&injection_mutex, K_FOREVER);
    flags = active_flags;
    k_mutex_unlock(&injection_mutex);
    return (enum fault_injection_flag)flags;
}

void fault_injector_apply(const struct plant_snapshot *normal,
                          struct fault_injection_result *result)
{
    const uint32_t flags = fault_injector_get_active();

    result->sample = *normal;
    result->valid = (flags & FAULT_INJECTION_SENSOR_FAILURE) == 0U;
    if (flags & FAULT_INJECTION_OVERTEMPERATURE) {
        result->sample.temperature_mdeg_c = FAULT_INJECTION_TEMPERATURE_MDEG_C;
    }
    if (flags & FAULT_INJECTION_VIBRATION) {
        result->sample.vibration_um_s = FAULT_INJECTION_VIBRATION_UM_S;
    }
    if (flags & FAULT_INJECTION_OVERCURRENT) {
        result->sample.current_ma = FAULT_INJECTION_CURRENT_MA;
    }
}
