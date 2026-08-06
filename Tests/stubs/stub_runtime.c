/*
 * Host 单元测试运行时桩:
 *   - 可测试控制的全局状态 (限位/时间/EEPROM/电机/配置)
 *   - bsp_motor / bsp_wdt / EEPROM / fault_log / config / printf 的桩实现
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "eeprom_config.h"
#include "fault_log.h"

/* ==================== 测试控制全局 ==================== */
uint32_t test_millis_now  = 0;
uint8_t  test_limit_open  = 0;
uint8_t  test_limit_close = 0;
uint8_t  test_motor_enable = 0;   /* M_SLP 状态 */
uint8_t  test_drive        = 0;   /* 0=停止 1=开阀 2=关阀 */
uint8_t  test_eeprom[4096];
uint32_t test_fault_count  = 0;
uint8_t  test_config_saved = 0;

/* ==================== 配置全局 (原 eeprom_config.c 定义) ==================== */
uint8_t g_valve_last_order = 0;
ValveDelayConfig_t g_vdelay = {0, 0};
PressureThresholdConfig_t g_pthreshold  = {0};
PressureThresholdConfig_t g_pthreshold2 = {0};
TempThresholdConfig_t g_tthreshold  = {0};
TempThresholdConfig_t g_tthreshold2 = {0};
VoltagePowerConfig_t g_spupower = {0};

/* ==================== 串口/时间桩 ==================== */
int printf_debug(const char *fmt, ...)
{
    (void)fmt;
    return 0;   /* 测试时静默; 需要输出可改 #define TEST_PRINTF */
}

uint32_t trea_millis(void)
{
    return test_millis_now;
}

void trea_delay_ms(uint32_t ms)
{
    test_millis_now += ms;   /* 模拟时间流逝 (阻塞式 valve_execute 的压紧延时使用) */
}

/* ==================== bsp_motor 桩 ==================== */
uint8_t g_motor_enabled = 1;

void bsp_motor_init(void) { }
uint8_t bsp_motor_limit_open(void)  { return test_limit_open; }
uint8_t bsp_motor_limit_close(void) { return test_limit_close; }
void bsp_motor_enable(uint8_t on)   { test_motor_enable = on; }
void bsp_motor_drive_open(void)     { test_drive = 1; }
void bsp_motor_drive_close(void)    { test_drive = 2; }
void bsp_motor_drive_stop(void)     { test_drive = 0; }

/* ==================== bsp_wdt 桩 ==================== */
void bsp_wdt_sw_enable(uint32_t ms) { (void)ms; }
void bsp_wdt_sw_feed(void)          { }
void bsp_wdt_sw_disable(void)       { }
void bsp_wdt_ota_enable(void)       { }
void bsp_wdt_ota_feed(void)         { }

/* ==================== EEPROM 桩 (RAM 镜像) ==================== */
uint8_t M24M01_Write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++)
        test_eeprom[(addr + i) & 0x0FFF] = data[i];
    return 0;
}

uint8_t M24M01_Read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++)
        buf[i] = test_eeprom[(addr + i) & 0x0FFF];
    return 0;
}

/* ==================== fault_log / config 桩 ==================== */
uint8_t fault_log_add(FaultCode_t code, uint32_t param)
{
    (void)code;
    (void)param;
    test_fault_count++;
    return 0;
}

void config_save_ext(void)
{
    test_config_saved = 1;
}
