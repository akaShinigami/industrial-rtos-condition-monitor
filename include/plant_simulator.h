#ifndef PLANT_SIMULATOR_H_
#define PLANT_SIMULATOR_H_

#include "plant_model.h"

/** Initialize and start the periodic plant-simulation service. */
void plant_simulator_init(void);

/** Copy a mutex-protected coherent measurement snapshot into @p snapshot. */
void plant_simulator_get_snapshot(struct plant_snapshot *snapshot);

#endif /* PLANT_SIMULATOR_H_ */
