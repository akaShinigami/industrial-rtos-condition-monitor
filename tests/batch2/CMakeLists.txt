cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(batch2_tests)

target_sources(app PRIVATE
    src/main.c
    src/test_fault_injector.c
    src/test_diagnostics.c
    src/test_integration.c
    ../../src/plant_model.c
    ../../src/controller_state.c
    ../../src/fault_injector.c
    ../../src/diagnostics.c
    ../../src/condition_monitor.c
    ../../src/actuator_state_machine.c
    ../../src/safety_supervisor.c
    ../../src/sensor_health.c
    ../../src/monitoring_pipeline.c
)
target_include_directories(app PRIVATE ../../include)
