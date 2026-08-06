/**
 * @file    sched.c
 * @brief   系统事件寄存器实现
 *
 * 并发模型与旧标志一致: ISR 置位为单次写, 主循环读取+清零;
 * 竞态窗口为微秒级, 语义与旧实现相同 (不引入额外关中断开销).
 */

#include "sched.h"

static volatile uint32_t s_events = 0;

void sched_event_set(uint32_t bits)
{
    s_events |= bits;
}

uint32_t sched_event_get(void)
{
    uint32_t e = s_events;
    s_events = 0;
    return e;
}

uint32_t sched_event_pending(void)
{
    return s_events;
}

void sched_event_clear(uint32_t bits)
{
    s_events &= ~bits;
}
