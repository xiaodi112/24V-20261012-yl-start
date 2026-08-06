/**
 * @file    fault_log.h
 * @brief   故障与复位原因记录 (EEPROM 环形缓冲)
 *
 * 存储区: 0x0500 ~ 0x059F (M24M01 128KB / AT24C64 8KB 通用, 配置区内空闲段)
 *   管理头 32B @0x0500 : magic(2) + version(1) + count(1) + head(1) + reserved(27)
 *   记录区 8条 x 16B @0x0520
 *
 * 调用约定:
 *   fault_log_capture_reset_cause()  上电最早调用 (必须在 rcu_all_reset_flag_clear 之前)
 *   fault_log_init()                 RTC 就绪后调用 (写入本次复位记录)
 *   fault_log_add()                  运行时记录故障
 */
#ifndef FAULT_LOG_H
#define FAULT_LOG_H

#include <stdint.h>

/* ==================== 故障码 ==================== */
typedef enum {
    FAULT_CODE_RESET            = 0x01,   /* 上电/复位记录 (reset_cause 有效) */
    FAULT_CODE_RTC_NOT_COUNTING = 0x02,   /* 深睡前 RTC 不走时, 睡眠被阻止 */
    FAULT_CODE_VALVE_TIMEOUT    = 0x03,   /* 阀门动作超时 (P3 使用) */
    FAULT_CODE_VALVE_ERR        = 0x04,   /* 阀门故障: 双限位/机械卡死 */
    FAULT_CODE_VALVE_RECOVER    = 0x05    /* 上电恢复动作中断的阀门指令 */
} FaultCode_t;

/* ==================== 复位原因 ==================== */
typedef enum {
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_POWER_ON,   /* 上电复位 (含欠压 BOR) */
    RESET_CAUSE_PIN,        /* 外部复位引脚 */
    RESET_CAUSE_SOFT,       /* 软件复位 NVIC_SystemReset (含软件看门狗触发) */
    RESET_CAUSE_FWDGT,      /* 硬件看门狗 FWDGT */
    RESET_CAUSE_WWDGT,      /* 窗口看门狗 WWDGT */
    RESET_CAUSE_LP,         /* 低功耗复位 */
    RESET_CAUSE_V11         /* V11 复位 */
} ResetCause_t;

/* ==================== 故障记录 (16B, 页对齐) ==================== */
typedef struct {
    uint8_t  code;          /* FaultCode_t */
    uint8_t  reset_cause;   /* ResetCause_t (RESET 记录有效) */
    uint8_t  year;          /* BCD */
    uint8_t  month;         /* BCD */
    uint8_t  date;          /* BCD */
    uint8_t  hour;          /* BCD */
    uint8_t  minute;        /* BCD */
    uint8_t  second;        /* BCD */
    uint32_t param;         /* 附加参数 */
    uint8_t  crc8;          /* 前13字节 CRC8 (poly 0x07) */
    uint8_t  reserved[2];
} FaultRecord_t;            /* 16 bytes */

/* ==================== 接口 ==================== */

/* 捕获本次复位原因 (读取 RCC 复位标志, 须在标志被清除前调用) */
void fault_log_capture_reset_cause(void);

/* 初始化环形区并写入本次复位记录 (RTC 就绪后调用, 幂等) */
void fault_log_init(void);

/* 追加一条故障记录 (自动带时间戳), 返回 0=成功 */
uint8_t fault_log_add(FaultCode_t code, uint32_t param);

/* 当前有效记录数 (0~8) */
uint16_t fault_log_count(void);

/* 读取第 index 条记录 (0=最旧), 返回 0=成功 1=越界 2=CRC错 */
uint8_t fault_log_read(uint16_t index, FaultRecord_t *out);

/* 清空全部故障记录 */
void fault_log_clear(void);

/* 本次复位原因 */
ResetCause_t fault_log_reset_cause(void);

/* 复位原因字符串 (调试输出用, ASCII) */
const char* fault_log_reset_cause_str(ResetCause_t cause);

#endif /* FAULT_LOG_H */
