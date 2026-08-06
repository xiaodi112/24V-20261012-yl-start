/**
 * @file    sensor_ctx.h
 * @brief   传感器全局上下文 (双路原始数据 + 电压 + 状态跟踪)
 *
 * 从 main.h 拆出, 供 main/ec800e/downlink 等模块共享, 避免业务模块反向依赖 main.h.
 */
#ifndef SENSOR_CTX_H
#define SENSOR_CTX_H

#include "PT_304_35.h"

typedef struct {
    PT304D_Data raw;
    PT304D_Data raw2;
    float       voltage;
    float       last_pressure_collect;
    float       last_pressure_realtime;
    float       last_pressure_collect2;
    float       last_pressure_realtime2;
    float       last_normal_voltage;
    uint8_t     prev_online;
    uint8_t     offline_cnt;
    uint8_t     online_cnt;
    uint8_t     prev_online2;
    uint8_t     offline_cnt2;
    uint8_t     online_cnt2;
} SensorContext_t;

extern SensorContext_t g_sensor;

#endif /* SENSOR_CTX_H */
