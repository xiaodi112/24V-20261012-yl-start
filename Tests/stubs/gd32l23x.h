#ifndef GD32L23X_H
#define GD32L23X_H
/*
 * Host 单元测试桩: 替代真实 GD32L23x 芯片头文件.
 * 被测模块 (threshold/json_builder/sensor_buffer/valve_fsm) 仅依赖
 * stdint 类型与 printf_debug, 不直接访问寄存器, 因此桩可以为空.
 */
#include <stdint.h>
#endif
