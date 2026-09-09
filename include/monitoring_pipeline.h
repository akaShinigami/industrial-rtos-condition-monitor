#ifndef MONITORING_PIPELINE_H_
#define MONITORING_PIPELINE_H_

#include "plant_model.h"

/** Deterministic cycle: inject, assess valid measurements, update validity, supervise. */
void monitoring_pipeline_process(const struct plant_snapshot *normal);

#endif /* MONITORING_PIPELINE_H_ */
