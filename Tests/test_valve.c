/* Host unit tests for blocking valve_execute (merged safety logic).
 * Limit/time/motor state is driven through test globals plus a per-iteration
 * hook (valve_test_hook) so debounce/timing behavior can be scripted.
 */
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
extern uint8_t test_config_saved;
extern uint32_t test_millis_now;
extern ValveDelayConfig_t g_vdelay;
extern uint8_t g_valve_last_order;
extern void (*volatile valve_test_hook)(void);

#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)

static int hook_calls;
static int hook_done;

static void reset(void)
{
    valve_test_hook = 0;
    hook_calls = 0;
    hook_done = 0;
    g_motor_enabled = 1;
    test_limit_open = 0;
    test_limit_close = 0;
    test_motor_enable = 0;
    test_drive = 0;
    test_millis_now = 0;
    test_fault_count = 0;
    test_config_saved = 0;
    g_valve_last_order = 0;
    memset(&g_vdelay, 0, sizeof(g_vdelay));
}

/* normal open: target arrives, needs 150ms continuous validity */
static void hook_normal_open(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 50; break;                        /* t=50 moving */
    case 2: test_limit_open = 1; test_millis_now += 50; break;   /* t=100 tracking start */
    case 3: test_millis_now += 50; break;                        /* t=150: 50ms */
    case 4: test_millis_now += 50; break;                        /* t=200: 100ms */
    case 5: test_millis_now += 50; break;                        /* t=250: 150ms -> done */
    default: hook_done = 1; break;
    }
}

static void test_normal_open(void)
{
    reset();
    valve_test_hook = hook_normal_open;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 5);
    CHECK(hook_done == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(g_valve_last_order == 1);
    CHECK(test_config_saved == 1);
    CHECK(test_fault_count == 0);
}

/* move timeout -> valve_timeout + fault log + stop */
static void hook_timeout(void)
{
    hook_calls++;
    if(hook_calls == 1) test_millis_now += 20001;
}

static void test_timeout(void)
{
    reset();
    valve_test_hook = hook_timeout;
    const char *r = valve_execute("close", 0);
    CHECK(strcmp(r, "valve_timeout") == 0);
    CHECK(hook_calls == 1);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 1);
    CHECK(test_config_saved == 0);   /* update_last_order=0 */
}

/* both limits active at start: refuse without energizing */
static void test_both_limits_precheck(void)
{
    reset();
    test_limit_open = 1;
    test_limit_close = 1;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_err") == 0);
    CHECK(hook_calls == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 1);
}

/* both limits active during move: immediate stop */
static void hook_both_during(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 500; break;
    case 2: test_limit_open = 1; test_limit_close = 1; test_millis_now += 50; break;
    default: hook_done = 1; break;
    }
}

static void test_both_limits_during(void)
{
    reset();
    valve_test_hook = hook_both_during;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_err") == 0);
    CHECK(hook_calls == 2);
    CHECK(test_motor_enable == 0);
    CHECK(test_fault_count == 1);
}

/* already at target: completes immediately (no debounce needed) */
static void hook_already_target(void)
{
    hook_calls++;
}

static void test_already_at_target(void)
{
    reset();
    test_limit_open = 1;
    valve_test_hook = hook_already_target;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 1);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 0);
}

/* wrong-direction limit during move -> valve_err after 150ms */
static void hook_wrong_fault(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 500; break;                          /* moving */
    case 2: test_limit_close = 1; test_millis_now += 50; break;     /* wrong tracking start t=550 */
    case 3: test_millis_now += 50; break;                           /* 50ms */
    case 4: test_millis_now += 50; break;                           /* 100ms */
    case 5: test_millis_now += 50; break;                           /* 150ms -> fault */
    default: hook_done = 1; break;
    }
}

static void test_wrong_limit_fault(void)
{
    reset();
    valve_test_hook = hook_wrong_fault;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_err") == 0);
    CHECK(hook_calls == 5);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 1);
}

/* start from opposite end: leaving-position wrong limit ignored until released */
static void hook_start_opposite(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 100; break;                          /* wrong still active, ignored */
    case 2: test_limit_close = 0; test_millis_now += 500; break;    /* released -> armed */
    case 3: test_millis_now += 100; break;                          /* moving */
    case 4: test_limit_open = 1; test_millis_now += 50; break;      /* target tracking start t=750 */
    case 5: test_millis_now += 50; break;                           /* 50ms */
    case 6: test_millis_now += 50; break;                           /* 100ms */
    case 7: test_millis_now += 50; break;                           /* 150ms -> done */
    default: hook_done = 1; break;
    }
}

static void test_start_from_opposite(void)
{
    reset();
    test_limit_close = 1;
    valve_test_hook = hook_start_opposite;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 7);
    CHECK(test_fault_count == 0);
}

/* wrong limit released then re-activated -> detection re-arms, faults */
static void hook_wrong_rearm(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 100; break;                          /* wrong active, ignored */
    case 2: test_limit_close = 0; test_millis_now += 300; break;    /* released -> armed */
    case 3: test_limit_close = 1; test_millis_now += 50; break;     /* re-activate, tracking start t=450 */
    case 4: test_millis_now += 50; break;                           /* 50ms */
    case 5: test_millis_now += 50; break;                           /* 100ms */
    case 6: test_millis_now += 50; break;                           /* 150ms -> fault */
    default: hook_done = 1; break;
    }
}

static void test_wrong_rearm(void)
{
    reset();
    test_limit_close = 1;
    valve_test_hook = hook_wrong_rearm;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_err") == 0);
    CHECK(hook_calls == 6);
    CHECK(test_fault_count == 1);
}

/* transient target pulse < 150ms must not complete */
static void hook_target_debounce(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 100; break;
    case 2: test_limit_open = 1; test_millis_now += 50; break;      /* t=150 tracking start */
    case 3: test_limit_open = 0; test_millis_now += 50; break;      /* t=200 released -> reset */
    case 4: test_limit_open = 1; test_millis_now += 50; break;      /* t=250 re-track */
    case 5: test_millis_now += 50; break;                           /* 50ms */
    case 6: test_millis_now += 50; break;                           /* 100ms */
    case 7: test_millis_now += 50; break;                           /* 150ms -> done */
    default: hook_done = 1; break;
    }
}

static void test_target_debounce(void)
{
    reset();
    valve_test_hook = hook_target_debounce;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 7);
    CHECK(test_fault_count == 0);
}

/* transient wrong-limit pulse < 150ms must not fault */
static void hook_wrong_debounce(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 500; break;
    case 2: test_limit_close = 1; test_millis_now += 50; break;     /* wrong tracking start t=550 */
    case 3: test_limit_close = 0; test_millis_now += 50; break;     /* released -> reset */
    case 4: test_millis_now += 50; break;                           /* moving */
    case 5: test_limit_open = 1; test_millis_now += 50; break;      /* target tracking t=700 */
    case 6: test_millis_now += 50; break;                           /* 50ms */
    case 7: test_millis_now += 50; break;                           /* 100ms */
    case 8: test_millis_now += 50; break;                           /* 150ms -> done */
    default: hook_done = 1; break;
    }
}

static void test_wrong_debounce(void)
{
    reset();
    valve_test_hook = hook_wrong_debounce;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 8);
    CHECK(test_fault_count == 0);
}

/* press-and-hold phase: motor keeps driving during hold, stops after */
static void hook_hold(void)
{
    hook_calls++;
    switch(hook_calls)
    {
    case 1: test_millis_now += 100; break;
    case 2: test_limit_open = 1; test_millis_now += 50; break;      /* tracking start t=150 */
    case 3: test_millis_now += 50; break;                           /* 50ms */
    case 4: test_millis_now += 50; break;                           /* 100ms */
    case 5: test_millis_now += 50; break;                           /* 150ms -> break into hold */
    default: hook_done = 1; break;
    }
}

static void test_hold_phase(void)
{
    reset();
    g_vdelay.enabled = 1;
    g_vdelay.delay_ms = 1000;
    valve_test_hook = hook_hold;
    const char *r = valve_execute("open", 1);
    CHECK(strcmp(r, "valve_on") == 0);
    CHECK(hook_calls == 5);
    CHECK(test_millis_now == 1300);   /* 300ms move + 1000ms hold */
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
    CHECK(test_fault_count == 0);
}

/* invalid action / motor disabled contract */
static void test_invalid_and_disabled(void)
{
    reset();
    CHECK(strcmp(valve_execute("sideways", 1), "valve_err") == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);

    reset();
    g_motor_enabled = 0;
    CHECK(strcmp(valve_execute("open", 1), "valve_err") == 0);
    CHECK(strcmp(valve_execute("close", 0), "valve_err") == 0);
    CHECK(strcmp(valve_get_status(), "valve_err") == 0);
    CHECK(test_motor_enable == 0);
    CHECK(test_drive == 0);
}

/* status query via limits */
static void test_status_query(void)
{
    reset();
    g_motor_enabled = 1;
    test_limit_open = 1; test_limit_close = 0;
    CHECK(strcmp(valve_get_status(), "valve_on") == 0);
    test_limit_open = 0; test_limit_close = 1;
    CHECK(strcmp(valve_get_status(), "valve_off") == 0);
    test_limit_open = 1; test_limit_close = 1;
    CHECK(strcmp(valve_get_status(), "valve_err") == 0);
    test_limit_open = 0; test_limit_close = 0;
    CHECK(strcmp(valve_get_status(), "valve_timeout") == 0);
}

void test_valve_all(void)
{
    test_normal_open();
    test_timeout();
    test_both_limits_precheck();
    test_both_limits_during();
    test_already_at_target();
    test_wrong_limit_fault();
    test_start_from_opposite();
    test_wrong_rearm();
    test_target_debounce();
    test_wrong_debounce();
    test_hold_phase();
    test_invalid_and_disabled();
    test_status_query();
}
