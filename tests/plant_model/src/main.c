#include <zephyr/ztest.h>

#include "plant_model.h"

static void step_model(struct plant_model *model, bool running, uint32_t count)
{
    for (uint32_t index = 0U; index < count; ++index) {
        plant_model_step(model, running);
    }
}

ZTEST(plant_model, test_initialization)
{
    struct plant_model model;
    struct plant_snapshot snapshot;

    plant_model_init(&model);
    plant_model_get_snapshot(&model, &snapshot);
    zassert_equal(snapshot.temperature_mdeg_c, PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C);
    zassert_equal(snapshot.vibration_um_s, 0U);
    zassert_equal(snapshot.current_ma, 0U);
    zassert_equal(snapshot.simulation_time_ms, 0U);
}

ZTEST(plant_model, test_running_produces_nominal_signals_and_heating)
{
    struct plant_model model;
    struct plant_snapshot snapshot;

    plant_model_init(&model);
    plant_model_step(&model, true);
    plant_model_get_snapshot(&model, &snapshot);

    zassert_equal(snapshot.current_ma, PLANT_MODEL_RUNNING_CURRENT_MA);
    zassert_equal(snapshot.vibration_um_s, PLANT_MODEL_RUNNING_VIBRATION_UM_S);
    zassert_true(snapshot.temperature_mdeg_c > PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C);
}

ZTEST(plant_model, test_temperature_has_thermal_inertia)
{
    struct plant_model model;
    struct plant_snapshot snapshot;

    plant_model_init(&model);
    plant_model_step(&model, true);
    plant_model_get_snapshot(&model, &snapshot);

    zassert_true(snapshot.temperature_mdeg_c < PLANT_MODEL_RUNNING_TEMPERATURE_MDEG_C);
}

ZTEST(plant_model, test_running_temperature_is_bounded_and_trends_to_target)
{
    struct plant_model model;
    struct plant_snapshot early;
    struct plant_snapshot later;

    plant_model_init(&model);
    step_model(&model, true, 10U);
    plant_model_get_snapshot(&model, &early);
    step_model(&model, true, 200U);
    plant_model_get_snapshot(&model, &later);

    zassert_true(later.temperature_mdeg_c > early.temperature_mdeg_c);
    zassert_true(later.temperature_mdeg_c <= PLANT_MODEL_RUNNING_TEMPERATURE_MDEG_C);
    zassert_equal(later.current_ma, PLANT_MODEL_RUNNING_CURRENT_MA);
    zassert_equal(later.vibration_um_s, PLANT_MODEL_RUNNING_VIBRATION_UM_S);
}

ZTEST(plant_model, test_stop_cools_without_immediate_temperature_reset)
{
    struct plant_model model;
    struct plant_snapshot warm;
    struct plant_snapshot cooled;

    plant_model_init(&model);
    step_model(&model, true, 40U);
    plant_model_get_snapshot(&model, &warm);
    plant_model_step(&model, false);
    plant_model_get_snapshot(&model, &cooled);

    zassert_equal(cooled.current_ma, 0U);
    zassert_equal(cooled.vibration_um_s, 0U);
    zassert_true(cooled.temperature_mdeg_c < warm.temperature_mdeg_c);
    zassert_true(cooled.temperature_mdeg_c > PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C);
}

ZTEST(plant_model, test_stopped_temperature_never_cools_below_ambient)
{
    struct plant_model model;
    struct plant_snapshot snapshot;

    plant_model_init(&model);
    step_model(&model, true, 100U);
    step_model(&model, false, 500U);
    plant_model_get_snapshot(&model, &snapshot);

    zassert_true(snapshot.temperature_mdeg_c >= PLANT_MODEL_AMBIENT_TEMPERATURE_MDEG_C);
}

ZTEST(plant_model, test_model_is_deterministic)
{
    struct plant_model first;
    struct plant_model second;
    struct plant_snapshot first_snapshot;
    struct plant_snapshot second_snapshot;

    plant_model_init(&first);
    plant_model_init(&second);
    step_model(&first, true, 37U);
    step_model(&second, true, 37U);
    step_model(&first, false, 11U);
    step_model(&second, false, 11U);
    plant_model_get_snapshot(&first, &first_snapshot);
    plant_model_get_snapshot(&second, &second_snapshot);

    zassert_equal(first_snapshot.temperature_mdeg_c, second_snapshot.temperature_mdeg_c);
    zassert_equal(first_snapshot.vibration_um_s, second_snapshot.vibration_um_s);
    zassert_equal(first_snapshot.current_ma, second_snapshot.current_ma);
    zassert_equal(first_snapshot.simulation_time_ms, second_snapshot.simulation_time_ms);
}

ZTEST(plant_model, test_simulation_time_progresses_by_fixed_step)
{
    struct plant_model model;
    struct plant_snapshot snapshot;

    plant_model_init(&model);
    step_model(&model, false, 3U);
    plant_model_get_snapshot(&model, &snapshot);
    zassert_equal(snapshot.simulation_time_ms, 3U * PLANT_MODEL_STEP_MS);
}

ZTEST_SUITE(plant_model, NULL, NULL, NULL, NULL, NULL);
