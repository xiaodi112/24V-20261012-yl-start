#include "valve.h"
#include "bsp_motor.h"
#include "eeprom_config.h"
#include "delay.h"
#include "usart.h"
#include "app_config.h"
#include <string.h>

#if defined(VALVE_MODE_NONBLOCKING)
#include "valve_fsm.h"
#include "bsp_wdt.h"

// ==================== 阀门动作执行 (非阻塞状态机 + 协作式等待) ====================
// 由 valve_fsm 状态机驱动; 本函数在等待期间持续喂狗并推进状态机,
// 保持同步返回兼容接口 (valve_on/off/timeout/err); 异步路径走 valve_fsm_start 直调.
const char* valve_execute(const char *action, uint8_t update_last_order)
{
    if(!valve_fsm_start(action, update_last_order, 0))
    {
        if(valve_fsm_result_pending())
            return valve_result_str(valve_fsm_result());
        return "valve_err";
    }

    /* 协作式等待: 移动期间推进状态机并喂狗, 防止 SWDT 被长动作饿死 */
    while(valve_fsm_busy())
    {
        valve_fsm_poll();
        bsp_wdt_sw_feed();
    }

    return valve_result_str(valve_fsm_result());
}

#else   /* VALVE_MODE_BLOCKING */

// ==================== 阀门动作执行 (含valve_err检测) ====================
const char* valve_execute(const char *action, uint8_t update_last_order)
{
    if(!g_motor_enabled)
    {
        printf_debug("[VALVE] Motor disabled, skip\r\n");
        return "valve_err";
    }

    uint32_t t;
    uint8_t target_limit, wrong_limit;
    uint8_t is_open  = (strcmp(action, "open") == 0 || strcmp(action, "1") == 0);
    uint8_t is_close = (strcmp(action, "close") == 0 || strcmp(action, "0") == 0);

    if(!is_open && !is_close)
        return "valve_err";

    // M_SLP HIGH: 使能电机驱动
    bsp_motor_enable(1);

    if(is_open)
    {
        printf_debug("[SM] Valve OPEN\r\n");
        bsp_motor_drive_open();
        t = trea_millis();
        while(!bsp_motor_limit_open() && (trea_millis() - t) < 20000);

        // 到位后保持原动作 (delay_switch=1 时), 用于压紧阀芯
        // 安全 clamp: 即使 g_vdelay.delay_ms 被外部误修改, 也不超过 VDLY_DELAY_MS_MAX
        if(bsp_motor_limit_open() && g_vdelay.enabled && g_vdelay.delay_ms > 0)
        {
            uint16_t hold_ms = g_vdelay.delay_ms > VDLY_DELAY_MS_MAX ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
            printf_debug("[SM] Valve OPEN limit reached, hold %dms\r\n", hold_ms);
            trea_delay_ms(hold_ms);
        }

        bsp_motor_drive_stop();

        target_limit = bsp_motor_limit_open();
        wrong_limit  = bsp_motor_limit_close();
    }
    else
    {
        printf_debug("[SM] Valve CLOSE\r\n");
        bsp_motor_drive_close();
        t = trea_millis();
        while(!bsp_motor_limit_close() && (trea_millis() - t) < 20000);

        // 到位后保持原动作 (delay_switch=1 时), 用于压紧阀芯
        if(bsp_motor_limit_close() && g_vdelay.enabled && g_vdelay.delay_ms > 0)
        {
            uint16_t hold_ms = g_vdelay.delay_ms > VDLY_DELAY_MS_MAX ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
            printf_debug("[SM] Valve CLOSE limit reached, hold %dms\r\n", hold_ms);
            trea_delay_ms(hold_ms);
        }

        bsp_motor_drive_stop();

        target_limit = bsp_motor_limit_close();
        wrong_limit  = bsp_motor_limit_open();
    }

    // M_SLP LOW: 关闭电机驱动
    bsp_motor_enable(0);

    // valve_err: 两个限位同时触发, 或目标未到位且反向限位也未触发(机械卡死)
    if(target_limit && wrong_limit)
        return "valve_err";
    if(target_limit)
    {
        if(update_last_order) { g_valve_last_order = is_open ? 1 : 0; config_save_ext(); }
        return is_open ? "valve_on" : "valve_off";
    }
    if(wrong_limit)
        return "valve_err";
    return "valve_timeout";
}

#endif /* VALVE_MODE_* */

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
