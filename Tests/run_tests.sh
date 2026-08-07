#!/bin/sh
set -e
cd "$(dirname "$0")"
SRC_APP="../Pressure_Regulating_Box/App"; SRC_MIDDLE="../Pressure_Regulating_Box/Middle"; SRC_BSP="../Pressure_Regulating_Box/BSP"; SRC_DRV="../Pressure_Regulating_Box/Drivers"
gcc -std=c99 -Wall -DUNIT_TEST_HOOK -I stubs -I "$SRC_APP" -I "$SRC_MIDDLE" -I "$SRC_BSP" -I "$SRC_DRV" \
    test_main.c test_json.c test_sensor_buffer.c test_threshold.c test_valve.c \
    stubs/stub_runtime.c \
    "$SRC_APP/valve.c" \
    "$SRC_MIDDLE/json_builder.c" "$SRC_MIDDLE/sensor_buffer.c" "$SRC_MIDDLE/threshold.c" \
    -o tests
./tests
