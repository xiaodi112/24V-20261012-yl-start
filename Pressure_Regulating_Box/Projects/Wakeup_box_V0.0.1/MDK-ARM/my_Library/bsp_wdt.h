/**
 * @file    bsp_wdt.h
 * @brief   看门狗 BSP 接口
 *
 * 策略保持现状不变:
 *   - 软件看门狗: 活跃阶段使能 (180s), 深睡前关闭
 *   - 硬件 FWDGT: 仅 OTA 期间启用
 */
#ifndef BSP_WDT_H
#define BSP_WDT_H

#include <stdint.h>

void bsp_wdt_sw_enable(uint32_t timeout_ms);   /* 使能软件看门狗并复位计数 */
void bsp_wdt_sw_feed(void);                    /* 喂软件看门狗 */
void bsp_wdt_sw_disable(void);                 /* 关闭软件看门狗 (深睡前) */

void bsp_wdt_ota_enable(void);                 /* 使能硬件 FWDGT (~32s, OTA 用) */
void bsp_wdt_ota_feed(void);                   /* 喂硬件 FWDGT */

#endif /* BSP_WDT_H */
