#ifndef SENSOR_HEALTH_H_
#define SENSOR_HEALTH_H_

#include <stdbool.h>

/** Replace only SENSOR_FAILURE and recompute aggregate controller health. */
void sensor_health_update(bool valid);

#endif /* SENSOR_HEALTH_H_ */
