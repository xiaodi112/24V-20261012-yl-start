/**
 * @file    valve_fsm.h
 * @brief   阀门动作非阻塞状态机 (安全核心)
 *
 * 状态: IDLE / OPENING / CLOSING / HOLDING(压紧) / FAULT
 * 安全设计:
 *   - 动作前限位预检 (双限位同时触发直接故障, 不使能电机)
 *   - 移动中超时 / 双限位 / 反向限位触发 → 立即停止并记录故障
 *   - 压紧(HOLD)阶段带独立超时
 *   - 动作前写 EEPROM 日志, 完成后清除; 上电时 valve_fsm_recover() 恢复中断动作
 *   - valve_fsm_poll() 由主循环周期调用, 动作全程喂狗由调用方负责
 */
#ifndef VALVE_FSM_H
#define VALVE_FSM_H

#include <stdint.h>

/* ==================== 状态 ==================== */
typedef enum {
    VALVE_FSM_IDLE = 0,
    VALVE_FSM_OPENING,
    VALVE_FSM_CLOSING,
    VALVE_FSM_HOLDING,
    VALVE_FSM_FAULT
} ValveFsmState_t;

/* ==================== 动作结果 ==================== */
typedef enum {
    VALVE_FSM_RESULT_NONE = 0,
    VALVE_FSM_RESULT_ON,       /* valve_on */
    VALVE_FSM_RESULT_OFF,      /* valve_off */
    VALVE_FSM_RESULT_TIMEOUT,  /* valve_timeout */
    VALVE_FSM_RESULT_ERR       /* valve_err */
} ValveFsmResult_t;

/* ==================== 接口 ==================== */

/* 初始化 (上电调用一次) */
void valve_fsm_init(void);

/**
 * 启动一次阀门动作 (非阻塞)
 * @param action "open"/"1" 或 "close"/"0"
 * @param update_last_order 完成后是否持久化 g_valve_last_order
 * @param preempt 1=允许抢占当前反方向动作 (告警关阀用); 0=忙时拒绝
 * @return 1=已接受, 0=拒绝 (动作无效/电机禁用/双限位故障/忙且不抢占)
 */
uint8_t valve_fsm_start(const char *action, uint8_t update_last_order, uint8_t preempt);

/* 主循环周期调用: 推进状态机 (限位/超时/压紧/故障处理) */
void valve_fsm_poll(void);

/* 是否有动作在进行中 */
uint8_t valve_fsm_busy(void);

/* 是否有完成结果待读取 */
uint8_t valve_fsm_result_pending(void);

/* 读取并清除完成结果 */
ValveFsmResult_t valve_fsm_result(void);

/* 上电恢复: 检测 EEPROM 动作日志, 续走中断的动作 */
void valve_fsm_recover(void);

/* 结果转字符串 (与旧 valve_execute 返回串一致) */
const char* valve_result_str(ValveFsmResult_t result);

#endif /* VALVE_FSM_H */
