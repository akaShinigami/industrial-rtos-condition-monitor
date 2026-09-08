cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(controller_state_tests)

target_sources(app PRIVATE src/main.c ../../src/controller_state.c)
target_include_directories(app PRIVATE ../../include)
