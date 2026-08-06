/* 电机功能开关契约测试 (g_motor_enabled) */
#include <stdio.h>
#include <string.h>
#include "valve.h"
#include "eeprom_config.h"

extern int g_test_failures;
extern uint8_t g_motor_enabled;
extern uint8_t test_limit_open;
extern uint8_t test_limit_close;
extern uint8_t test_motor_enable;
extern uint8_t test_drive;
extern uint32_t test_fault_count;
extern ValveDelayConfig_t g_vdelay;

#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)

static void reset(void)
{
    g_motor_enabled = 1;
    test_limit_open = 0;
    test_limit_close = 0;
    test_motor_enable = 0;
    test_drive = 0;
    test_fault_count = 0;
    memset(&g_vdelay, 0, sizeof(g_vdelay));
}

/* 契约: g_motor_enabled=0 时, 电机动作与阀门状态上报全部失效 (返回 valve_err) */
static void test_motor_disabled_contract(void)
{
    reset();
    g_motor_enabled = 0;

    /* 动作失效 */
    CHECK(strcmp(valve_execute("open", 1), "valve_err") == 0);
    CHECK(strcmp(valve_execute("close", 0), "valve_err") == 0);
    /* 状态上报失效 */
    CHECK(strcmp(valve_get_status(), "valve_err") == 0);
    /* 电机从未被使能/驱动 */
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
}

/* 契约: g_motor_enabled=1 时, 动作与状态正常 */
static void test_motor_enabled_status(void)
{
    reset();
    g_motor_enabled = 1;

    /* 限位组合 → 状态查询 */
    test_limit_open = 1; test_limit_close = 0;
    CHECK(strcmp(valve_get_status(), "valve_on") == 0);
    test_limit_open = 0; test_limit_close = 1;
    CHECK(strcmp(valve_get_status(), "valve_off") == 0);
    test_limit_open = 1; test_limit_close = 1;
    CHECK(strcmp(valve_get_status(), "valve_err") == 0);
    test_limit_open = 0; test_limit_close = 0;
    CHECK(strcmp(valve_get_status(), "valve_timeout") == 0);

    /* 动作正常 (阻塞式): 目标限位已到位 → valve_on */
    reset();
    test_limit_open = 1;
    CHECK(strcmp(valve_execute("open", 1), "valve_on") == 0);
    CHECK(g_valve_last_order == 1);
    CHECK(test_motor_enable == 0);
}

void test_valve_all(void)
{
    test_motor_disabled_contract();
    test_motor_enabled_status();
}
