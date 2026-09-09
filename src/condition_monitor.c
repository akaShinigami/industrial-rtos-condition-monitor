#include "condition_monitor.h"
static enum condition_level level(int32_t v,int32_t w,int32_t f){return v>=f?CONDITION_FAULT:v>=w?CONDITION_WARNING:CONDITION_NORMAL;}
static enum condition_level unsigned_level(uint32_t value, uint32_t warning, uint32_t fault)
{
    return value >= fault ? CONDITION_FAULT :
        value >= warning ? CONDITION_WARNING : CONDITION_NORMAL;
}
void condition_monitor_evaluate(const struct plant_snapshot*s,struct condition_assessment*a){a->temperature=level(s->temperature_mdeg_c,CONDITION_TEMPERATURE_WARNING_MDEG_C,CONDITION_TEMPERATURE_FAULT_MDEG_C);a->vibration=unsigned_level(s->vibration_um_s,CONDITION_VIBRATION_WARNING_UM_S,CONDITION_VIBRATION_FAULT_UM_S);a->current=unsigned_level(s->current_ma,CONDITION_CURRENT_WARNING_MA,CONDITION_CURRENT_FAULT_MA);a->fault_flags=0;if(a->temperature==CONDITION_FAULT)a->fault_flags|=CONTROLLER_FAULT_OVERTEMPERATURE;if(a->vibration==CONDITION_FAULT)a->fault_flags|=CONTROLLER_FAULT_EXCESSIVE_VIBRATION;if(a->current==CONDITION_FAULT)a->fault_flags|=CONTROLLER_FAULT_OVERCURRENT;a->health=a->fault_flags?CONTROLLER_HEALTH_FAULT:(a->temperature||a->vibration||a->current)?CONTROLLER_HEALTH_WARNING:CONTROLLER_HEALTH_HEALTHY;}
void condition_monitor_apply(const struct plant_snapshot *sample)
{
    struct condition_assessment assessment;

    condition_monitor_evaluate(sample, &assessment);
    controller_state_update_process(assessment.fault_flags,
                                    assessment.health == CONTROLLER_HEALTH_WARNING);
}
