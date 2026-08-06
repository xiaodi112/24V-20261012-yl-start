/**
 * @file    bsp_motor.h
 * @brief   电机驱动 BSP 接口 (阀门执行机构)
 *
 * 业务层 (valve.c) 只依赖本接口, 不接触寄存器/引脚;
 * 驱动方式由 motor.h 的 MOTOR_DRV_GPIO / MOTOR_DRV_PWM 编译宏选择.
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdint.h>

/* 电机功能开关 (产品级配置):
 *   0 = 电机功能全部禁用 (动作与阀门状态上报均失效, 返回 valve_err)
 *   1 = 电机功能启用 (默认值由产品配置决定, 见 bsp_motor.c) */
extern uint8_t g_motor_enabled;

void bsp_motor_init(void);               /* 引脚初始化 (含 M_SLP 使能脚, 初始关闭) */
uint8_t bsp_motor_limit_open(void);      /* 1=开阀到位 (K1-S LOW) */
uint8_t bsp_motor_limit_close(void);     /* 1=关阀到位 (K2-S LOW) */
void bsp_motor_enable(uint8_t on);       /* M_SLP: 1=使能驱动芯片 */
void bsp_motor_drive_open(void);         /* 开阀方向 */
void bsp_motor_drive_close(void);        /* 关阀方向 */
void bsp_motor_drive_stop(void);         /* 两路停止 */

#endif /* BSP_MOTOR_H */
