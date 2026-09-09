cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(industrial_rtos_condition_monitor)

target_sources(app PRIVATE
    src/main.c
    src/controller_state.c
    src/plant_model.c
    src/plant_simulator.c
    src/condition_monitor.c
    src/condition_monitor_service.c
    src/actuator_state_machine.c
    src/safety_supervisor.c
)

target_include_directories(app PRIVATE include)
