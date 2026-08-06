/* 阀门状态机安全逻辑单元测试 */
#include <stdio.h>
#include <string.h>
#include "valve_fsm.h"
#include "eeprom_config.h"

extern uint32_t test_millis_now;
extern uint8_t  test_limit_open;
extern uint8_t  test_limit_close;
extern uint8_t  test_motor_enable;
extern uint8_t  test_drive;
extern uint32_t test_fault_count;
extern uint8_t  test_config_saved;
extern uint8_t  g_motor_enabled;
extern ValveDelayConfig_t g_vdelay;
extern uint8_t  g_valve_last_order;

extern int g_test_failures;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)
#define RESET_VALVE() do { \
    valve_fsm_init(); \
    test_limit_open = 0; test_limit_close = 0; \
    test_motor_enable = 0; test_drive = 0; \
    test_millis_now = 0; test_fault_count = 0; test_config_saved = 0; \
    g_motor_enabled = 1; g_valve_last_order = 0; \
    memset(&g_vdelay, 0, sizeof(g_vdelay)); \
    memset(test_eeprom, 0xFF, 4096); \
} while(0)

extern uint8_t test_eeprom[4096];

/* 正常开阀: 到位 → ON, 停止使能, 持久化 */
static void test_normal_open(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("open", 1, 0) == 1);
    CHECK(valve_fsm_busy() == 1);
    CHECK(test_motor_enable == 1);
    CHECK(test_drive == 1);

    test_millis_now += 1000;
    test_limit_open = 1;
    valve_fsm_poll();

    CHECK(valve_fsm_result_pending() == 1);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ON);
    CHECK(valve_fsm_busy() == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(g_valve_last_order == 1);
    CHECK(test_config_saved == 1);
    CHECK(test_fault_count == 0);
}

/* 移动超时 → TIMEOUT + 故障记录 + 停机 */
static void test_timeout(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("close", 0, 0) == 1);
    test_millis_now += 20001;
    valve_fsm_poll();

    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_TIMEOUT);
    CHECK(valve_fsm_busy() == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 1);
}

/* 双限位预检: 拒绝启动, 电机从不使能 */
static void test_both_limits_precheck(void)
{
    RESET_VALVE();
    test_limit_open = 1;
    test_limit_close = 1;

    CHECK(valve_fsm_start("open", 1, 0) == 0);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ERR);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 1);
}

/* 移动中反向限位触发 → 机械故障 */
static void test_wrong_limit_fault(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("open", 1, 0) == 1);
    test_millis_now += 500;
    test_limit_close = 1;   /* 开阀方向却触发关阀限位 */
    valve_fsm_poll();

    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ERR);
    CHECK(valve_fsm_busy() == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_fault_count == 1);
}

/* 压紧阶段: 到位后保持使能, 超时后完成 */
static void test_hold_phase(void)
{
    RESET_VALVE();
    g_vdelay.enabled = 1;
    g_vdelay.delay_ms = 1000;

    CHECK(valve_fsm_start("open", 1, 0) == 1);
    test_millis_now += 1000;
    test_limit_open = 1;
    valve_fsm_poll();

    CHECK(valve_fsm_busy() == 1);          /* 压紧中 */
    CHECK(test_motor_enable == 1);         /* 压紧期间保持使能 */
    CHECK(test_drive == 0);                /* 但已停转 */

    test_millis_now += 1001;
    valve_fsm_poll();
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ON);
    CHECK(test_motor_enable == 0);
    CHECK(test_fault_count == 0);
}

/* 抢占: 告警关阀可打断正在进行的开阀 */
static void test_preempt(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("open", 1, 0) == 1);
    test_millis_now += 300;
    CHECK(valve_fsm_start("close", 0, 1) == 1);   /* 抢占 */
    CHECK(test_drive == 2);

    test_millis_now += 20001;
    valve_fsm_poll();
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_TIMEOUT);
    CHECK(valve_fsm_busy() == 0);
}

/* 忙时拒绝: 反方向非抢占 → 拒绝并出 ERR */
static void test_busy_refuse(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("open", 0, 0) == 1);
    CHECK(valve_fsm_start("close", 0, 0) == 0);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ERR);
}

/* 失电恢复: 动作中断后重新上电, recover 续走并完成 */
static void test_power_loss_recovery(void)
{
    RESET_VALVE();
    /* 启动开阀 (写入 EEPROM 日志), 模拟中途掉电 */
    CHECK(valve_fsm_start("open", 1, 0) == 1);
    test_millis_now += 500;

    /* 重新上电: 状态机复位, 但 EEPROM 日志保留 */
    valve_fsm_init();
    test_limit_open = 1;   /* 目标已到位, recover 首轮即完成 */
    valve_fsm_recover();

    CHECK(valve_fsm_busy() == 0);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ON);
    CHECK(test_motor_enable == 0);
    CHECK(test_fault_count == 1);   /* VALVE_RECOVER 故障记录 */
}

/* 非法动作 / 电机禁用 */
static void test_invalid_and_disabled(void)
{
    RESET_VALVE();
    CHECK(valve_fsm_start("sideways", 1, 0) == 0);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ERR);

    RESET_VALVE();
    g_motor_enabled = 0;
    CHECK(valve_fsm_start("open", 1, 0) == 0);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ERR);
    CHECK(test_motor_enable == 0);
}

/* 上报来源 set/take */
static void test_report_origin(void)
{
    RESET_VALVE();
    valve_fsm_set_report(VALVE_REPORT_ALARM);
    CHECK(valve_fsm_start("open", 0, 0) == 1);
    test_limit_open = 1;
    valve_fsm_poll();
    CHECK(valve_fsm_result_pending() == 1);
    CHECK(valve_fsm_take_report() == VALVE_REPORT_ALARM);
    CHECK(valve_fsm_result() == VALVE_FSM_RESULT_ON);
}

void test_valve_fsm_all(void)
{
    test_normal_open();
    test_timeout();
    test_both_limits_precheck();
    test_wrong_limit_fault();
    test_hold_phase();
    test_preempt();
    test_busy_refuse();
    test_power_loss_recovery();
    test_invalid_and_disabled();
    test_report_origin();
}
