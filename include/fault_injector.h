#ifndef FAULT_INJECTOR_H_
#define FAULT_INJECTOR_H_

#include "plant_model.h"

enum fault_injection_flag {
    FAULT_INJECTION_NONE = 0U,
    FAULT_INJECTION_OVERTEMPERATURE = 1U << 0,
    FAULT_INJECTION_VIBRATION = 1U << 1,
    FAULT_INJECTION_OVERCURRENT = 1U << 2,
    FAULT_INJECTION_SENSOR_FAILURE = 1U << 3,
};

/* Deterministic simulation values, not universal industrial limits. */
#define FAULT_INJECTION_TEMPERATURE_MDEG_C 70000
#define FAULT_INJECTION_VIBRATION_UM_S 4500U
#define FAULT_INJECTION_CURRENT_MA 8000U

struct fault_injection_result {
    struct plant_snapshot sample;
    bool valid;
};

/** Initialize once before starting workers. Configuration calls are thread-safe. */
void fault_injector_init(void);
/** Flags may be OR-combined; unsupported bits are ignored. */
void fault_injector_enable(enum fault_injection_flag flags);
void fault_injector_disable(enum fault_injection_flag flags);
void fault_injector_clear_all(void);
enum fault_injection_flag fault_injector_get_active(void);
/** Copy normal into result, then overlay one coherent injection configuration. */
void fault_injector_apply(const struct plant_snapshot *normal,
                          struct fault_injection_result *result);

#endif /* FAULT_INJECTOR_H_ */
