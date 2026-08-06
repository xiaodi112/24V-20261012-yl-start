/**
 * @file    valve_fsm.c
 * @brief   阀门动作非阻塞状态机实现
 *
 * 堵转保护说明: 当前硬件仅有双限位开关, 无电流检测;
 * 保护由三层构成: 动作前双限位预检 / 移动中双限位与反向限位即时故障 / 移动超时.
 * 若未来增加电流采样或驱动芯片 FAULT 引脚, 在 poll() 中追加判定即可.
 */

#include "valve_fsm.h"
#include "bsp_motor.h"
#include "bsp_wdt.h"
#include "eeprom_config.h"
#include "delay.h"
#include "fault_log.h"
#include "m24m01.h"
#include "usart.h"
#include <string.h>

/* ==================== 时间预算 ==================== */
#define VALVE_MOVE_TIMEOUT_MS   20000U   /* 单程移动超时 (与旧实现一致) */

/* ==================== EEPROM 动作日志 (失电恢复) ==================== */
#define VALVE_JOURNAL_BASE      0x05A0U
#define VALVE_JOURNAL_MAGIC     0xA5F1U

typedef struct {
    uint16_t magic;
    uint8_t  is_open;      /* 1=开阀, 0=关阀 */
    uint8_t  persist;      /* 完成后是否持久化 last_order */
    uint8_t  reserved[12];
} ValveJournal_t;          /* 16B */

/* ==================== 状态机上下文 ==================== */
typedef struct {
    ValveFsmState_t state;
    uint8_t  is_open;           /* 目标方向 */
    uint8_t  persist;
    uint32_t move_start_ms;     /* 移动起始时刻 */
    uint32_t hold_deadline_ms;  /* 压紧结束时刻 */
    ValveFsmResult_t result;
    uint8_t  result_pending;
    ValveReportOrigin_t report_origin;
} ValveFsmCtx_t;

static ValveFsmCtx_t s_vfsm;

/* ==================== 内部: EEPROM 日志 ==================== */
static void journal_save(uint8_t is_open, uint8_t persist)
{
    ValveJournal_t j;

    memset(&j, 0, sizeof(j));
    j.magic   = VALVE_JOURNAL_MAGIC;
    j.is_open = is_open;
    j.persist = persist;
    M24M01_Write(VALVE_JOURNAL_BASE, (const uint8_t *)&j, sizeof(j));
}

static void journal_clear(void)
{
    ValveJournal_t j;

    memset(&j, 0xFF, sizeof(j));
    M24M01_Write(VALVE_JOURNAL_BASE, (const uint8_t *)&j, sizeof(j));
}

static uint8_t journal_load(ValveJournal_t *out)
{
    ValveJournal_t j;

    M24M01_Read(VALVE_JOURNAL_BASE, (uint8_t *)&j, sizeof(j));
    if(j.magic != VALVE_JOURNAL_MAGIC)
        return 0;
    *out = j;
    return 1;
}

/* ==================== 内部: 故障收口 ==================== */
static void vfsm_fault(ValveFsmResult_t result, FaultCode_t code)
{
    bsp_motor_drive_stop();
    bsp_motor_enable(0);
    journal_clear();

    fault_log_add(code, (uint32_t)(s_vfsm.is_open ? 1 : 0));
    printf_debug("[VALVE] fault result=%d dir=%d\r\n",
                 (int)result, (int)(s_vfsm.is_open ? 1 : 0));

    s_vfsm.state = VALVE_FSM_IDLE;
    s_vfsm.result = result;
    s_vfsm.result_pending = 1;
}

/* ==================== 内部: 完成收口 ==================== */
static void vfsm_complete(ValveFsmResult_t result)
{
    bsp_motor_drive_stop();
    bsp_motor_enable(0);
    journal_clear();

    if(s_vfsm.persist)
    {
        g_valve_last_order = (result == VALVE_FSM_RESULT_ON) ? 1 : 0;
        config_save_ext();
    }

    printf_debug("[VALVE] done result=%d\r\n", (int)result);
    s_vfsm.state = VALVE_FSM_IDLE;
    s_vfsm.result = result;
    s_vfsm.result_pending = 1;
}

/* ==================== 内部: 限位读取 ==================== */
static uint8_t limit_target(void)
{
    return s_vfsm.is_open ? bsp_motor_limit_open() : bsp_motor_limit_close();
}

static uint8_t limit_wrong(void)
{
    return s_vfsm.is_open ? bsp_motor_limit_close() : bsp_motor_limit_open();
}

/* ==================== 接口实现 ==================== */
void valve_fsm_init(void)
{
    memset(&s_vfsm, 0, sizeof(s_vfsm));
    s_vfsm.state = VALVE_FSM_IDLE;
    s_vfsm.report_origin = VALVE_REPORT_NONE;
}

void valve_fsm_set_report(ValveReportOrigin_t origin)
{
    s_vfsm.report_origin = origin;
}

ValveReportOrigin_t valve_fsm_take_report(void)
{
    ValveReportOrigin_t o = s_vfsm.report_origin;

    s_vfsm.report_origin = VALVE_REPORT_NONE;
    return o;
}

uint8_t valve_fsm_start(const char *action, uint8_t update_last_order, uint8_t preempt)
{
    uint8_t is_open;
    uint8_t target, wrong;

    if(!action)
        return 0;

    if(strcmp(action, "open") == 0 || strcmp(action, "1") == 0)
        is_open = 1;
    else if(strcmp(action, "close") == 0 || strcmp(action, "0") == 0)
        is_open = 0;
    else
        return 0;

    if(!g_motor_enabled)
    {
        printf_debug("[VALVE] Motor disabled, skip\r\n");
        s_vfsm.result = VALVE_FSM_RESULT_ERR;
        s_vfsm.result_pending = 1;
        return 0;
    }

    /* 忙时处理: 同方向重复指令视为已接受; 反方向需 preempt 抢占 */
    if(s_vfsm.state != VALVE_FSM_IDLE)
    {
        if(s_vfsm.is_open == is_open)
            return 1;   /* 已在向目标方向动作, 完成时统一出结果 */
        if(!preempt)
        {
            s_vfsm.result = VALVE_FSM_RESULT_ERR;
            s_vfsm.result_pending = 1;
            return 0;
        }
        /* 抢占: 先停止当前动作, 再重启反方向 */
        bsp_motor_drive_stop();
        bsp_motor_enable(0);
    }

    /* 动作前限位预检: 双限位同时触发 = 机械故障, 不使能电机 */
    target = limit_target();
    wrong  = limit_wrong();
    if(target && wrong)
    {
        printf_debug("[VALVE] both limits active, abort without energizing\r\n");
        journal_clear();   /* 动作无法进行, 清除恢复日志避免下次上电重试 */
        fault_log_add(FAULT_CODE_VALVE_ERR, 0);
        s_vfsm.result = VALVE_FSM_RESULT_ERR;
        s_vfsm.result_pending = 1;
        return 0;
    }

    /* 动作前写 EEPROM 日志 (失电恢复依据), 再使能电机 */
    journal_save(is_open, update_last_order);

    s_vfsm.is_open  = is_open;
    s_vfsm.persist  = update_last_order;
    s_vfsm.state    = is_open ? VALVE_FSM_OPENING : VALVE_FSM_CLOSING;
    s_vfsm.move_start_ms = trea_millis();

    bsp_motor_enable(1);
    if(is_open)
        bsp_motor_drive_open();
    else
        bsp_motor_drive_close();

    printf_debug("[VALVE] start %s (persist=%d)\r\n", is_open ? "OPEN" : "CLOSE",
                 (int)update_last_order);
    return 1;
}

void valve_fsm_poll(void)
{
    uint32_t now;
    uint8_t target, wrong;

    switch(s_vfsm.state)
    {
        case VALVE_FSM_OPENING:
        case VALVE_FSM_CLOSING:
            now = trea_millis();
            target = limit_target();
            wrong  = limit_wrong();

            /* 双限位同时触发: 机械故障, 立即停止 */
            if(target && wrong)
            {
                vfsm_fault(VALVE_FSM_RESULT_ERR, FAULT_CODE_VALVE_ERR);
                break;
            }
            /* 目标到位 */
            if(target)
            {
                bsp_motor_drive_stop();
                /* 压紧阶段: 保持使能, 带独立超时 */
                if(g_vdelay.enabled && g_vdelay.delay_ms > 0)
                {
                    uint32_t hold_ms = (g_vdelay.delay_ms > VDLY_DELAY_MS_MAX)
                                     ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
                    s_vfsm.state = VALVE_FSM_HOLDING;
                    s_vfsm.hold_deadline_ms = now + hold_ms;
                    printf_debug("[VALVE] limit reached, hold %lums\r\n", (unsigned long)hold_ms);
                }
                else
                {
                    vfsm_complete(s_vfsm.is_open ? VALVE_FSM_RESULT_ON : VALVE_FSM_RESULT_OFF);
                }
                break;
            }
            /* 反向限位触发 (目标未到位): 机械卡死/方向错误 */
            if(wrong)
            {
                vfsm_fault(VALVE_FSM_RESULT_ERR, FAULT_CODE_VALVE_ERR);
                break;
            }
            /* 移动超时 (无限位变化, 等价堵转判定) */
            if((now - s_vfsm.move_start_ms) >= VALVE_MOVE_TIMEOUT_MS)
            {
                vfsm_fault(VALVE_FSM_RESULT_TIMEOUT, FAULT_CODE_VALVE_TIMEOUT);
            }
            break;

        case VALVE_FSM_HOLDING:
            now = trea_millis();
            /* 压紧期间双限位异常: 立即故障 */
            if(limit_target() && limit_wrong())
            {
                vfsm_fault(VALVE_FSM_RESULT_ERR, FAULT_CODE_VALVE_ERR);
                break;
            }
            if(now >= s_vfsm.hold_deadline_ms)
            {
                /* 压紧结束: 目标限位仍有效 → 成功; 否则按旧语义判定 */
                if(limit_target())
                    vfsm_complete(s_vfsm.is_open ? VALVE_FSM_RESULT_ON : VALVE_FSM_RESULT_OFF);
                else if(limit_wrong())
                    vfsm_fault(VALVE_FSM_RESULT_ERR, FAULT_CODE_VALVE_ERR);
                else
                    vfsm_fault(VALVE_FSM_RESULT_TIMEOUT, FAULT_CODE_VALVE_TIMEOUT);
            }
            break;

        case VALVE_FSM_IDLE:
        case VALVE_FSM_FAULT:
        default:
            break;
    }
}

uint8_t valve_fsm_busy(void)
{
    return (s_vfsm.state != VALVE_FSM_IDLE) ? 1 : 0;
}

uint8_t valve_fsm_result_pending(void)
{
    return s_vfsm.result_pending;
}

ValveFsmResult_t valve_fsm_result(void)
{
    ValveFsmResult_t r = s_vfsm.result;

    s_vfsm.result = VALVE_FSM_RESULT_NONE;
    s_vfsm.result_pending = 0;
    return r;
}

void valve_fsm_recover(void)
{
    ValveJournal_t j;

    if(!journal_load(&j))
        return;

    printf_debug("[VALVE] recovering interrupted move (dir=%s)\r\n",
                 j.is_open ? "OPEN" : "CLOSE");
    fault_log_add(FAULT_CODE_VALVE_RECOVER, (uint32_t)(j.is_open ? 1 : 0));

    /* 续走中断动作: 目标限位与超时保护兜底 */
    valve_fsm_start(j.is_open ? "open" : "close", j.persist, 0);

    /* 同步完成: 避免电机在后续 INIT 阻塞期间无人轮询而长时间带电 */
    while(valve_fsm_busy())
    {
        valve_fsm_poll();
        bsp_wdt_sw_feed();
    }
    printf_debug("[VALVE] recovered result=%s\r\n",
                 valve_result_str(valve_fsm_result()));
}

const char* valve_result_str(ValveFsmResult_t result)
{
    switch(result)
    {
        case VALVE_FSM_RESULT_ON:       return "valve_on";
        case VALVE_FSM_RESULT_OFF:      return "valve_off";
        case VALVE_FSM_RESULT_TIMEOUT:  return "valve_timeout";
        case VALVE_FSM_RESULT_ERR:      return "valve_err";
        default:                    return "valve_err";
    }
}
