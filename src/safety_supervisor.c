#include "safety_supervisor.h"
#include "controller_state.h"
#include "actuator_state_machine.h"
void safety_supervisor_evaluate(void){struct controller_snapshot s;controller_state_get_snapshot(&s);if(s.health_state>=CONTROLLER_HEALTH_FAULT&&s.actuator_state!=CONTROLLER_ACTUATOR_FAULTED)actuator_state_machine_request(CONTROLLER_ACTUATOR_FAULTED);}
