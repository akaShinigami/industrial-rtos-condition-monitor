cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(plant_model_tests)

target_sources(app PRIVATE src/main.c ../../src/plant_model.c)
target_include_directories(app PRIVATE ../../include)
