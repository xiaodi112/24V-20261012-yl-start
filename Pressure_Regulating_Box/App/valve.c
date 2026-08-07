#include "valve.h"
#include "bsp_motor.h"
#include "eeprom_config.h"
#include "delay.h"
#include "usart.h"
#include "bsp_wdt.h"
#include "fault_log.h"
#include "app_config.h"
#include <string.h>

#if defined(UNIT_TEST_HOOK)
extern void (*volatile valve_test_hook)(void);
#endif

/* ==================== timing params ==================== */
#define VALVE_MOVE_TIMEOUT_MS   20000U   /* single-trip move timeout */
#define VALVE_LIMIT_DEBOUNCE_MS 150U     /* limit debounce window (continuous active ms) */

/* ==================== valve action execution (blocking) ====================
 * Safety logic ported from the non-blocking FSM (validated on target):
 *   - bsp_motor_init() before action: after deep-sleep wakeup the limit pins
 *     may be Analog and read as pressed, causing instant false completion
 *   - pre-action pre-check: both limits active = mechanical fault, never energize
 *   - target arrival / wrong-direction limit both need 150ms continuous validity;
 *     a limit already active at start is the "leaving position", its detection
 *     stays disarmed until that limit releases
 *   - 20s move timeout; software watchdog fed during the whole move
 *   - both-limits / wrong-limit / timeout -> immediate stop + fault log
 */
const char* valve_execute(const char *action, uint8_t update_last_order)
{
    uint32_t move_start_ms;
    uint32_t target_since_ms = 0;
    uint32_t wrong_since_ms  = 0;
    uint8_t  is_open, is_close;
    uint8_t  target, wrong;
    uint8_t  target_tracking = 0;
    uint8_t  wrong_tracking  = 0;
    uint8_t  target_at_start;
    uint8_t  wrong_armed;

    if(!g_motor_enabled)
    {
        printf_debug("[VALVE] Motor disabled, skip\r\n");
        return "valve_err";
    }

    is_open  = (strcmp(action, "open") == 0 || strcmp(action, "1") == 0);
    is_close = (strcmp(action, "close") == 0 || strcmp(action, "0") == 0);
    if(!is_open && !is_close)
        return "valve_err";

    /* ensure motor/limit pins are correctly configured (idempotent) */
    bsp_motor_init();

    /* pre-check using THIS action direction */
    target = is_open ? bsp_motor_limit_open() : bsp_motor_limit_close();
    wrong  = is_open ? bsp_motor_limit_close() : bsp_motor_limit_open();
    printf_debug("[VALVE] limits at start: open=%d close=%d (dir=%s)\r\n",
                 (int)bsp_motor_limit_open(), (int)bsp_motor_limit_close(),
                 is_open ? "OPEN" : "CLOSE");

    /* both limits active = mechanical fault: do not energize */
    if(target && wrong)
    {
        printf_debug("[VALVE] both limits active, abort without energizing\r\n");
        fault_log_add(FAULT_CODE_VALVE_ERR, 0);
        return "valve_err";
    }

    /* target already active -> complete directly;
     * wrong limit active at start = leaving position, detection disarmed until release */
    target_at_start = target;
    wrong_armed     = wrong ? 0 : 1;

    bsp_motor_enable(1);
    if(is_open)
        bsp_motor_drive_open();
    else
        bsp_motor_drive_close();
    move_start_ms = trea_millis();

    while(1)
    {
        uint32_t now;

        /* keep software watchdog alive during the whole move */
        bsp_wdt_sw_feed();
#if defined(UNIT_TEST_HOOK)
        if(valve_test_hook) valve_test_hook();
#endif

        now = trea_millis();
        target = is_open ? bsp_motor_limit_open() : bsp_motor_limit_close();
        wrong  = is_open ? bsp_motor_limit_close() : bsp_motor_limit_open();

        /* both limits active: mechanical fault, stop immediately */
        if(target && wrong)
        {
            bsp_motor_drive_stop();
            bsp_motor_enable(0);
            fault_log_add(FAULT_CODE_VALVE_ERR, (uint32_t)(is_open ? 1 : 0));
            return "valve_err";
        }

        /* target arrival: already-at-start completes directly;
         * otherwise needs continuous validity >= debounce window */
        if(target)
        {
            if(target_at_start)
                break;
            if(!target_tracking)
            {
                target_tracking  = 1;
                target_since_ms = now;
            }
            else if((now - target_since_ms) >= VALVE_LIMIT_DEBOUNCE_MS)
                break;
        }
        else
        {
            target_tracking = 0;
            target_since_ms = 0;
        }

        /* wrong-direction limit (target not reached): edge + debounce = jam/wrong direction;
         * a limit active at start is ignored until it releases (leaving position) */
        if(wrong)
        {
            if(wrong_armed)
            {
                if(!wrong_tracking)
                {
                    wrong_tracking  = 1;
                    wrong_since_ms = now;
                }
                else if((now - wrong_since_ms) >= VALVE_LIMIT_DEBOUNCE_MS)
                {
                    bsp_motor_drive_stop();
                    bsp_motor_enable(0);
                    fault_log_add(FAULT_CODE_VALVE_ERR, (uint32_t)(is_open ? 1 : 0));
                    return "valve_err";
                }
            }
        }
        else
        {
            wrong_tracking = 0;
            wrong_since_ms = 0;
            wrong_armed    = 1;   /* released -> re-arm detection */
        }

        /* move timeout (no limit change = stall equivalent) */
        if((now - move_start_ms) >= VALVE_MOVE_TIMEOUT_MS)
        {
            bsp_motor_drive_stop();
            bsp_motor_enable(0);
            fault_log_add(FAULT_CODE_VALVE_TIMEOUT, (uint32_t)(is_open ? 1 : 0));
            return "valve_timeout";
        }
    }

    /* arrival: optional press-and-hold phase (keep driving), clamped for safety */
    if(g_vdelay.enabled && g_vdelay.delay_ms > 0)
    {
        uint16_t hold_ms = g_vdelay.delay_ms > VDLY_DELAY_MS_MAX ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
        printf_debug("[VALVE] limit reached, hold %dms\r\n", (int)hold_ms);
        trea_delay_ms(hold_ms);
    }

    bsp_motor_drive_stop();
    bsp_motor_enable(0);

    if(update_last_order)
    {
        g_valve_last_order = is_open ? 1 : 0;
        config_save_ext();
    }

    printf_debug("[VALVE] done result=%s\r\n", is_open ? "valve_on" : "valve_off");
    return is_open ? "valve_on" : "valve_off";
}

// ==================== 阀门实时状态查询 (不动作) ====================
const char* valve_get_status(void)
{
    /* 电机功能禁用时, 动作与状态上报全部失效 (契约: g_motor_enabled=0) */
    if(!g_motor_enabled)
        return "valve_err";

    uint8_t l1 = bsp_motor_limit_open();   // 开阀到位
    uint8_t l2 = bsp_motor_limit_close();  // 关阀到位

    if(l1 && l2)  return "valve_err";
    if(l1)        return "valve_on";
    if(l2)        return "valve_off";
    return "valve_timeout";
}
