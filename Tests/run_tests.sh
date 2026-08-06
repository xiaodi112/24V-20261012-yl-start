#!/bin/sh
# Host 单元测试: 编译并运行 (需要 gcc)
set -e
cd "$(dirname "$0")"
SRC_MIDDLE="../Pressure_Regulating_Box/Middle"; SRC_BSP="../Pressure_Regulating_Box/BSP"; SRC_DRV="../Pressure_Regulating_Box/Drivers"
gcc -std=c99 -Wall -I stubs -I "$SRC_MIDDLE" -I "$SRC_BSP" -I "$SRC_DRV" \
    test_main.c test_json.c test_sensor_buffer.c test_threshold.c test_valve_fsm.c \
    stubs/stub_runtime.c \
    "$SRC_MIDDLE/json_builder.c" "$SRC_MIDDLE/sensor_buffer.c" "$SRC_MIDDLE/threshold.c" "$SRC_MIDDLE/valve_fsm.c" \
    -o tests
./tests
