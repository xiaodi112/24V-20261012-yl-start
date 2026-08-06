#include "valve.h"
#include "bsp_motor.h"
#include "valve_fsm.h"
#include "bsp_wdt.h"

// ==================== 阀门动作执行 (含valve_err检测) ====================
// 由 valve_fsm 非阻塞状态机驱动; 本函数在等待期间持续喂狗并推进状态机,
// 保持与旧调用方同步返回的兼容接口 (valve_on/off/timeout/err).
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

// ==================== 阀门实时状态查询 (不动作) ====================
const char* valve_get_status(void)
{
    uint8_t l1 = bsp_motor_limit_open();   // 开阀到位
    uint8_t l2 = bsp_motor_limit_close();  // 关阀到位

    if(l1 && l2)  return "valve_err";
    if(l1)        return "valve_on";
    if(l2)        return "valve_off";
    return "valve_timeout";
}
