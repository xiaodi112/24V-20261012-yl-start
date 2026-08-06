/**
 * @file    sched.h
 * @brief   系统事件调度: 事件位寄存器 (ISR 置位, 主循环统一消费)
 *
 * 替代原有分散的唤醒标志 (rtc_alarm_flag/counter0/button_wakeup_flag/
 * ri_wakeup_flag/ble_connect_wakeup_flag), 语义一致:
 *   - ISR 只调用 sched_event_set() 置位, 不做业务处理
 *   - 主循环通过 sched_event_get() 一次性快照消费
 *   - 深睡等待用 sched_event_pending() (只读不消费)
 */
#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

/* ==================== 事件位定义 ==================== */
#define EVT_RTC         (1U << 0)   /* RTC 闹钟唤醒 */
#define EVT_LPUART      (1U << 1)   /* LPUART 起始位唤醒 (下行) */
#define EVT_BUTTON      (1U << 2)   /* 按键 EXTI0 */
#define EVT_RI          (1U << 3)   /* 4G RI EXTI1 */
#define EVT_BLE_CONN    (1U << 4)   /* BLE 连接 EXTI2 */

/* ==================== 接口 ==================== */

/* ISR 安全: 置位一个或多个事件 */
void sched_event_set(uint32_t bits);

/* 读取并清零全部事件 (主循环消费, 一次快照) */
uint32_t sched_event_get(void);

/* 只读当前事件 (深睡等待用, 不消费) */
uint32_t sched_event_pending(void);

/* 清除指定事件 */
void sched_event_clear(uint32_t bits);

#endif /* SCHED_H */
