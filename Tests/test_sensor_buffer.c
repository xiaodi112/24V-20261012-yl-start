/* 传感器环形缓冲单元测试 */
#include <stdio.h>
#include <string.h>
#include "sensor_buffer.h"

extern int g_test_failures;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_test_failures++; } } while(0)

static void test_ring(void)
{
    SensorRecord_t rec;
    uint16_t i;

    memset(&rec, 0, sizeof(rec));
    sensor_buf_init();
    CHECK(sensor_buf_count() == 0);

    for(i = 0; i < SENSOR_BUF_MAX_RECORDS; i++)
    {
        rec.pressure = (float)i;
        sensor_buf_push(&rec);
    }
    CHECK(sensor_buf_count() == SENSOR_BUF_MAX_RECORDS);

    /* 读第 0 条 = 最旧 */
    {
        const SensorRecord_t *p = sensor_buf_get(0);
        CHECK(p != 0);
        CHECK(p->pressure == 0.0f);
    }

    /* 覆盖最旧: 再压一条, 第 0 条变成 1.0 */
    rec.pressure = 999.0f;
    sensor_buf_push(&rec);
    {
        const SensorRecord_t *p = sensor_buf_get(0);
        CHECK(p != 0);
        CHECK(p->pressure == 1.0f);
    }
    CHECK(sensor_buf_count() == SENSOR_BUF_MAX_RECORDS);

    /* pop 与越界 */
    sensor_buf_pop();
    CHECK(sensor_buf_count() == SENSOR_BUF_MAX_RECORDS - 1);
    CHECK(sensor_buf_get(SENSOR_BUF_MAX_RECORDS) == 0);

    sensor_buf_clear();
    CHECK(sensor_buf_count() == 0);
}

void test_sensor_buffer_all(void)
{
    test_ring();
}
