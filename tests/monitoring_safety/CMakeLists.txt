cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(monitoring_safety_tests)

target_sources(app PRIVATE
    src/main.c
    src/test_condition_monitor.c
    src/test_actuator_state_machine.c
    src/test_safety_supervisor.c
    src/test_integration.c
    ../../src/controller_state.c
    ../../src/condition_monitor.c
    ../../src/actuator_state_machine.c
    ../../src/safety_supervisor.c
)
target_include_directories(app PRIVATE ../../include)
