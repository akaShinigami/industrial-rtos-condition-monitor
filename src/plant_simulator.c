#include <zephyr/kernel.h>

#include "controller_state.h"
#include "plant_simulator.h"

#define PLANT_SIMULATOR_STACK_SIZE 1024
#define PLANT_SIMULATOR_PRIORITY 5

static struct plant_model model;
static struct plant_snapshot published_snapshot;
K_MUTEX_DEFINE(snapshot_mutex);
K_SEM_DEFINE(initialization_complete, 0, 1);

static void plant_simulator_thread(void)
{
    struct controller_snapshot controller_snapshot;
    struct plant_snapshot next_snapshot;

    k_sem_take(&initialization_complete, K_FOREVER);

    while (true) {
        controller_state_get_snapshot(&controller_snapshot);
        plant_model_step(&model,
                         controller_snapshot.actuator_state == CONTROLLER_ACTUATOR_RUNNING);
        plant_model_get_snapshot(&model, &next_snapshot);

        k_mutex_lock(&snapshot_mutex, K_FOREVER);
        published_snapshot = next_snapshot;
        k_mutex_unlock(&snapshot_mutex);

        k_sleep(K_MSEC(PLANT_MODEL_STEP_MS));
    }
}

/* A 1024-byte, priority-5 worker keeps periodic simulation below main work. */
K_THREAD_DEFINE(plant_simulator_thread_id, PLANT_SIMULATOR_STACK_SIZE,
                plant_simulator_thread, NULL, NULL, NULL,
                PLANT_SIMULATOR_PRIORITY, 0, 0);

void plant_simulator_init(void)
{
    k_mutex_lock(&snapshot_mutex, K_FOREVER);
    plant_model_init(&model);
    plant_model_get_snapshot(&model, &published_snapshot);
    k_mutex_unlock(&snapshot_mutex);
    k_sem_give(&initialization_complete);
}

void plant_simulator_get_snapshot(struct plant_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    k_mutex_lock(&snapshot_mutex, K_FOREVER);
    *snapshot = published_snapshot;
    k_mutex_unlock(&snapshot_mutex);
}
