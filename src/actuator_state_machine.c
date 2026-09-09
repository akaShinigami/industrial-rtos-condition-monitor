#include "actuator_state_machine.h"
static int ok(enum controller_actuator_state a,enum controller_actuator_state b){return(a==0&&(b==1||b==4))||(a==1&&(b==2||b==4))||(a==2&&(b==3||b==4))||(a==3&&(b==0||b==4));}
enum actuator_transition_result actuator_state_machine_request(enum controller_actuator_state next)
{
    struct controller_snapshot snapshot;

    controller_state_get_snapshot(&snapshot);
    if (!ok(snapshot.actuator_state, next)) {
        return ACTUATOR_TRANSITION_INVALID;
    }
    /* Legality depends only on the actuator state. Recheck that exact state
     * atomically with the write so an intervening safety trip cannot be lost.
     */
    return controller_state_compare_exchange_actuator(snapshot.actuator_state, next) ?
        ACTUATOR_TRANSITION_OK : ACTUATOR_TRANSITION_INVALID;
}

enum actuator_transition_result actuator_state_machine_reset(void)
{
    return controller_state_try_reset_actuator() ?
        ACTUATOR_TRANSITION_OK : ACTUATOR_TRANSITION_RESET_DENIED;
}
