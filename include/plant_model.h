#ifndef PLANT_MODEL_H_
#define PLANT_MODEL_H_

#include <stdbool.h>
#include <stdint.h>

#define PLANT_MODEL_STEP_MS 100U
#define PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C 25000
#define PLANT_MODEL_RUNNING_TEMPERATURE_MDEG_C 45000
#define PLANT_MODEL_RUNNING_VIBRATION_UM_S 1500U
#define PLANT_MODEL_RUNNING_CURRENT_MA 4000U

/** A coherent set of simulation measurements in scaled engineering units. */
struct plant_snapshot {
    /** Temperature in milli-degrees Celsius (25000 = 25.000 C). */
    int32_t temperature_mdeg_c;
    /** Vibration velocity in micrometres per second (1500 = 1.500 mm/s). */
    uint32_t vibration_um_s;
    /** Electrical current in milliamps (4000 = 4.000 A). */
    uint32_t current_ma;
    /** Deterministic elapsed simulation time in milliseconds. */
    uint64_t simulation_time_ms;
};

/** Caller-owned state for the deterministic, fixed-step plant model. */
struct plant_model {
    struct plant_snapshot snapshot;
};

/** Initialize a model to ambient temperature with the actuator stopped. */
void plant_model_init(struct plant_model *model);

/** Advance exactly one 100 ms step for the supplied actuator running state. */
void plant_model_step(struct plant_model *model, bool actuator_running);

/** Copy the current model measurements into @p snapshot. */
void plant_model_get_snapshot(const struct plant_model *model,
                              struct plant_snapshot *snapshot);

#endif /* PLANT_MODEL_H_ */
