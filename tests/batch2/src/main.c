#include <zephyr/ztest.h>
#include "controller_state.h"
#include "fault_injector.h"
#include "diagnostics.h"

static void reset_modules(void *fixture)
{
    ARG_UNUSED(fixture);
    controller_state_init();
    fault_injector_init();
    diagnostics_init(0);
}

ZTEST_SUITE(fault_injector, NULL, NULL, reset_modules, NULL, NULL);
ZTEST_SUITE(diagnostics, NULL, NULL, reset_modules, NULL, NULL);
ZTEST_SUITE(batch2_integration, NULL, NULL, reset_modules, NULL, NULL);
