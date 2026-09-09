#include <stddef.h>

#include "plant_model.h"

#define TEMPERATURE_RESPONSE_DIVISOR 20

static int32_t move_temperature_toward(int32_t value, int32_t target)
{
    int32_t difference = target - value;
    int32_t change;

    if (difference == 0) {
        return value;
    }

    change = difference / TEMPERATURE_RESPONSE_DIVISOR;
    if (change == 0) {
        change = difference > 0 ? 1 : -1;
    }

    return value + change;
}

void plant_model_init(struct plant_model *model)
{
    if (model == NULL) {
        return;
    }

    model->snapshot.temperature_mdeg_c = PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C;
    model->snapshot.vibration_um_s = 0U;
    model->snapshot.current_ma = 0U;
    model->snapshot.simulation_time_ms = 0U;
}

void plant_model_step(struct plant_model *model, bool actuator_running)
{
    int32_t target_temperature;

    if (model == NULL) {
        return;
    }

    target_temperature = actuator_running ? PLANT_MODEL_RUNNING_TEMPERATURE_MDEG_C :
                                            PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C;
    model->snapshot.temperature_mdeg_c = move_temperature_toward(
        model->snapshot.temperature_mdeg_c, target_temperature);
    if (model->snapshot.temperature_mdeg_c < PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C) {
        model->snapshot.temperature_mdeg_c = PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C;
    }

    model->snapshot.current_ma = actuator_running ? PLANT_MODEL_RUNNING_CURRENT_MA : 0U;
    model->snapshot.vibration_um_s = actuator_running ? PLANT_MODEL_RUNNING_VIBRATION_UM_S : 0U;
    model->snapshot.simulation_time_ms += PLANT_MODEL_STEP_MS;
}

void plant_model_get_snapshot(const struct plant_model *model,
                              struct plant_snapshot *snapshot)
{
    if (model == NULL || snapshot == NULL) {
        return;
    }

    *snapshot = model->snapshot;
}
