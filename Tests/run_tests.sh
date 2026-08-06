#!/bin/sh
# Host 单元测试: 编译并运行 (需要 gcc)
set -e
cd "$(dirname "$0")"
SRC="../Pressure_Regulating_Box/Projects/Wakeup_box_V0.0.1/MDK-ARM/my_Library"
gcc -std=c99 -Wall -I stubs -I "$SRC" \
    test_main.c test_json.c test_sensor_buffer.c test_threshold.c test_valve_fsm.c \
    stubs/stub_runtime.c \
    "$SRC/json_builder.c" "$SRC/sensor_buffer.c" "$SRC/threshold.c" "$SRC/valve_fsm.c" \
    -o tests
./tests
