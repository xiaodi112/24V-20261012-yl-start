#ifndef EEPROM_HISTORY_H
#define EEPROM_HISTORY_H

#include <stdint.h>
#include "m24m01.h"    /* 取 EEPROM_DRV_* / EEPROM_TOTAL_SIZE 芯片选择 */

/**
 * @file eeprom_history.h
 * @brief EEPROM 历史数据环形缓冲存储 (M24M01 / AT24C64 编译选择)
 *
 * 地址规划 (按芯片容量自动选择):
 *   M24M01 (128KB):
 *   配置区  0x00000 ~ 0x00FFF (4KB 预留, 由 eeprom_config 使用)
 *   管理头  0x01000 ~ 0x010FF (8份 x 32B, 轮换写入实现磨损均衡, 占 1 页)
 *   历史区  0x01100 ~ 0x1FFFF (126720B = 3960条 x 32B)
 *   AT24C64 (8KB):
 *   配置区  0x00000 ~ 0x007FF (2KB 预留, 由 eeprom_config 使用)
 *   管理头  0x00800 ~ 0x008FF (8份 x 32B, 轮换写入实现磨损均衡)
 *   历史区  0x00900 ~ 0x01FFF (5888B = 184条 x 32B)
 *
 * 每条记录 32 字节, 页内对齐 (页大小由芯片决定: 256B / 32B), 写入无需跨页.
 * 环形覆盖: 写满后自动覆盖最旧记录.
 * 掉电恢复: 上电读取有效管理头副本恢复环形状态, 全部无效才格式化.
 */

// ==================== 地址定义 (按芯片自动选择) ====================
#if (EEPROM_TOTAL_SIZE >= 131072)          /* M24M01: 配置区 4KB */
  #define EE_HIST_BASE           0x01000   /* 历史区起始地址 */
  #define EE_HIST_HEADER_BASE    0x01000   /* 管理头基址 (8份副本, 每份32B) */
  #define EE_HIST_DATA_ADDR      0x01100   /* 数据区起始地址 (256B页对齐) */
  #define EE_HIST_END_ADDR       0x1FFFF   /* EEPROM末尾 */
#elif (EEPROM_TOTAL_SIZE >= 8192)          /* AT24C64: 配置区 2KB */
  #define EE_HIST_BASE           0x00800   /* 历史区起始地址 */
  #define EE_HIST_HEADER_BASE    0x00800   /* 管理头基址 (8份副本, 每份32B) */
  #define EE_HIST_DATA_ADDR      0x00900   /* 数据区起始地址 (32B页对齐) */
  #define EE_HIST_END_ADDR       0x01FFF   /* EEPROM末尾 */
#else
  #error "EEPROM_HISTORY: 不支持的 EEPROM 容量"
#endif
#define EE_HIST_HEADER_COPIES  8         /* 管理头副本数 (磨损均衡) */
#define EE_HIST_RECORD_SIZE    32        /* 每条记录字节数 */
#define EE_HIST_MAX_RECORDS    ((EE_HIST_END_ADDR - EE_HIST_DATA_ADDR + 1U) / EE_HIST_RECORD_SIZE)

// ==================== 管理头 Magic / 版本 ====================
#define EE_HIST_MAGIC          0xDA7A   /* "DATA" 谐音 */
#define EE_HIST_VERSION        1        /* 存储格式版本 (旧AT24C64格式=0, 版本不符自动重建) */

// ==================== 历史记录 (32字节, 页内对齐) ====================
typedef struct {
    float    pressure;       // 进气端压力 (Pa)          [4B]
    float    temperature;    // 进气端温度 (C)           [4B]
    float    pressure2;      // 出气端压力 (Pa)          [4B]
    float    temperature2;   // 出气端温度 (C)           [4B]
    float    voltage;        // 电池电压 (V)             [4B]
    uint8_t  ts_bcd[6];     // 时间戳 BCD: YY MM DD HH MM SS [6B]
    uint8_t  flags;          // 标志位 (SENSOR_FLAG_xxx) [1B]
    uint8_t  crc8;           // CRC8校验 (前31字节)      [1B]
    uint8_t  reserved[4];    // 预留扩展                 [4B]
} EeHistRecord_t;            // 合计: 32B

// ==================== 接口函数 ====================

/**
 * 初始化历史缓冲 (上电调用)
 * 优先恢复上次的管理头状态; 无有效副本时格式化.
 */
void ee_hist_init(void);

/**
 * 写入一条历史记录 (自动获取当前RTC时间作为时间戳)
 * @param pressure   进气端压力 (Pa)
 * @param temperature 进气端温度 (C)
 * @param pressure2  出气端压力 (Pa)
 * @param temperature2 出气端温度 (C)
 * @param voltage    电池电压 (V)
 * @param flags      标志位
 */
void ee_hist_push(float pressure, float temperature,
                  float pressure2, float temperature2,
                  float voltage, uint8_t flags);

/**
 * 读取第 index 条记录 (0=最旧, count-1=最新)
 * @param index  记录索引
 * @param out    输出缓冲
 * @return 0=成功, 1=索引越界, 2=CRC校验失败
 */
uint8_t ee_hist_read(uint16_t index, EeHistRecord_t *out);

/**
 * 获取当前有效记录数
 */
uint16_t ee_hist_count(void);

/**
 * 弹出最旧一条记录 (tail前进, count--, 保存管理头)
 * 用于逐条上报后删除
 */
void ee_hist_pop(void);

/**
 * 清空所有历史记录 (管理头复位, 不擦除数据页)
 */
void ee_hist_clear(void);

/**
 * 将 BCD 时间戳转为字符串 "20YY-MM-DD HH:MM:SS"
 * @param ts_bcd  6字节BCD时间 [YY,MM,DD,HH,MM,SS]
 * @param buf     输出缓冲 (至少20字节)
 */
void ee_hist_ts_to_str(const uint8_t *ts_bcd, char *buf);

/**
 * 将历史记录转为 SensorRecord_t 格式 (兼容现有上报接口)
 * @param hist   EEPROM历史记录
 * @param out    输出 SensorRecord_t (需包含 sensor_buffer.h)
 */
void ee_hist_to_sensor_record(const EeHistRecord_t *hist, void *out);

#endif /* EEPROM_HISTORY_H */
