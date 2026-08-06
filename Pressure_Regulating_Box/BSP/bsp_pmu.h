/**
 * @file    bsp_pmu.h
 * @brief   板级电源管理 BSP (深休眠/唤醒恢复/低功耗引脚)
 *
 * 从 usart.c / mygpio.c 拆出, 业务层只调用本接口, 不接触 PMU/时钟寄存器.
 */
#ifndef BSP_PMU_H
#define BSP_PMU_H

#include <stdint.h>

/**
 * 进入深休眠 (原 enter_deepsleep_mode)
 * @param lpuart_enabled 1=保留 LPUART 唤醒; 0=关闭
 * @param lcd_active     当前 LCD 是否显示 (用于决定 SLCD 引脚是否保留)
 * @param bt_on          当前蓝牙是否开启 (用于决定 BLE 引脚是否保留)
 */
void bsp_pmu_enter_deepsleep(uint8_t lpuart_enabled, uint8_t lcd_active, uint8_t bt_on);

/**
 * 唤醒后系统重新初始化 (时钟/SysTick/TIMER1/LPUART唤醒/RI)
 * @param lpuart_woke 本次是否为 LPUART 起始位唤醒 (需消费 RDATA 中的触发字节)
 */
void bsp_pmu_wakeup_reinit(uint8_t lpuart_woke);

/* 深睡前将未使用 GPIO 置为 Analog (原 gpio_enter_low_power) */
void bsp_pmu_low_power_pins(uint8_t lpuart_enabled, uint8_t lcd_active, uint8_t bt_on);

#endif /* BSP_PMU_H */
