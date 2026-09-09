#include <zephyr/ztest.h>
#include "controller_state.h"

static void reset_controller(void *fixture)
{
    ARG_UNUSED(fixture);
    controller_state_init();
}

ZTEST_SUITE(condition_monitor, NULL, NULL, reset_controller, NULL, NULL);
ZTEST_SUITE(actuator_state_machine, NULL, NULL, reset_controller, NULL, NULL);
ZTEST_SUITE(safety_supervisor, NULL, NULL, reset_controller, NULL, NULL);
ZTEST_SUITE(integration, NULL, NULL, reset_controller, NULL, NULL);
