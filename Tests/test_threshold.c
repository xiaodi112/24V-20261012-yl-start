/* 阈值判定逻辑单元测试 */
#include <stdio.h>
#include <string.h>
#include "threshold.h"
#include "eeprom_config.h"

extern PressureThresholdConfig_t g_pthreshold;
extern PressureThresholdConfig_t g_pthreshold2;
extern TempThresholdConfig_t g_tthreshold;
extern TempThresholdConfig_t g_tthreshold2;
extern VoltagePowerConfig_t g_spupower;

extern int g_test_failures;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)

static void setup_pressure(void)
{
    memset(&g_pthreshold, 0, sizeof(g_pthreshold));
    g_pthreshold.enabled = 1;
    g_pthreshold.ppmax = 1000.0f;
    g_pthreshold.pmax  = 500.0f;
    g_pthreshold.ppmin = -200.0f;
    g_pthreshold.pmin  = -100.0f;
    g_pthreshold.mutation = 300.0f;
    g_pthreshold.shock = 10.0f;
}

static void test_pressure_levels(void)
{
    memset(&g_spupower, 0, sizeof(g_spupower));
    setup_pressure();
    CHECK(threshold_check(1050.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_VERY_HIGH);
    CHECK(threshold_check(600.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_OVER_LIMIT);
    CHECK(threshold_check(-250.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_VERY_LOW);
    CHECK(threshold_check(-150.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_UNDER_LIMIT);
    CHECK(threshold_check(300.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_OK);
}

static void test_mutation(void)
{
    memset(&g_spupower, 0, sizeof(g_spupower));
    setup_pressure();
    /* last_pressure 必须 >=0 才判突变 (负值视为首采) */
    CHECK(threshold_check(450.0f, 25.0f, 3.6f, 100.0f, 1, 0) == THRESHOLD_MUTATION);
    /* skip_mutation=1 时不判突变 */
    CHECK(threshold_check(450.0f, 25.0f, 3.6f, 100.0f, 1, 1) == THRESHOLD_OK);
}

static void test_vprotect(void)
{
    setup_pressure();
    g_spupower.enabled = 1;
    g_spupower.protect = 3.2f;
    CHECK(threshold_check(300.0f, 25.0f, 3.0f, 0.0f, 1, 0) == THRESHOLD_VPROTECT);
    /* check_voltage=0 时电压不参与判定 */
    CHECK(threshold_check(300.0f, 25.0f, 3.0f, 0.0f, 0, 0) != THRESHOLD_VPROTECT);
    /* 禁用 spupower 时不触发 */
    g_spupower.enabled = 0;
    CHECK(threshold_check(300.0f, 25.0f, 3.0f, 0.0f, 1, 0) == THRESHOLD_OK);
}

static void test_temp_levels(void)
{
    memset(&g_pthreshold, 0, sizeof(g_pthreshold));
    memset(&g_spupower, 0, sizeof(g_spupower));
    memset(&g_tthreshold, 0, sizeof(g_tthreshold));
    g_tthreshold.enabled = 1;
    g_tthreshold.high  = 60.0f;
    g_tthreshold.low   = -10.0f;
    g_tthreshold.vhigh = 80.0f;
    g_tthreshold.vlow  = -20.0f;

    CHECK(threshold_check(0.0f, 90.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_TEMP_VERY_HIGH);
    CHECK(threshold_check(0.0f, 70.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_TEMP_HIGH);
    CHECK(threshold_check(0.0f, -30.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_TEMP_VERY_LOW);
    CHECK(threshold_check(0.0f, -15.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_TEMP_LOW);
    CHECK(threshold_check(0.0f, 25.0f, 3.6f, 0.0f, 1, 0) == THRESHOLD_OK);
}

static void test_status_strings(void)
{
    CHECK(threshold_pressure_status_str(PRESSURE_HIGH) != 0);
    CHECK(threshold_temp_status_str(TEMP_HIGH) != 0);
    CHECK(threshold_alarm_type_str(THRESHOLD_VERY_HIGH) != 0);
}

void test_threshold_all(void)
{
    test_pressure_levels();
    test_mutation();
    test_vprotect();
    test_temp_levels();
    test_status_strings();
}
