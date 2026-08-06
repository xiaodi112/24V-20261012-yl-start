/**
 * @file    bsp_wdt.c
 * @brief   看门狗 BSP 实现 (策略与现状一致)
 *
 * 软件看门狗实现位于 delay.c (SysTick 计数), 本模块为统一接口;
 * 硬件 FWDGT 由 ota_app.c 迁移至此, 仅 OTA 期间启用.
 */

#include "bsp_wdt.h"
#include "delay.h"
#include "gd32l23x.h"
#include "gd32l23x_fwdgt.h"

/* ===================== 软件看门狗 ===================== */
void bsp_wdt_sw_enable(uint32_t timeout_ms)
{
    swdt_enable(timeout_ms);
}

void bsp_wdt_sw_feed(void)
{
    swdt_feed();
}

void bsp_wdt_sw_disable(void)
{
    swdt_disable();
}

/* ===================== 硬件看门狗 FWDGT (OTA) ===================== */
void bsp_wdt_ota_enable(void)
{
    fwdgt_write_enable();
    fwdgt_config(4095, FWDGT_PSC_DIV256);
    fwdgt_counter_reload();
    fwdgt_enable();
}

void bsp_wdt_ota_feed(void)
{
    fwdgt_counter_reload();
}
