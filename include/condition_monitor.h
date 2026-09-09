#ifndef CONDITION_MONITOR_H
#define CONDITION_MONITOR_H
#include "controller_state.h"
#include "plant_model.h"
enum condition_level { CONDITION_NORMAL, CONDITION_WARNING, CONDITION_FAULT };
struct condition_assessment { enum condition_level temperature, vibration, current; enum controller_health_state health; uint32_t fault_flags; };
#define CONDITION_TEMPERATURE_WARNING_MDEG_C 50000
#define CONDITION_TEMPERATURE_FAULT_MDEG_C 60000
#define CONDITION_VIBRATION_WARNING_UM_S 2500U
#define CONDITION_VIBRATION_FAULT_UM_S 3500U
#define CONDITION_CURRENT_WARNING_MA 5500U
#define CONDITION_CURRENT_FAULT_MA 7000U
void condition_monitor_evaluate(const struct plant_snapshot *, struct condition_assessment *);
void condition_monitor_apply(const struct plant_snapshot *);
#endif
