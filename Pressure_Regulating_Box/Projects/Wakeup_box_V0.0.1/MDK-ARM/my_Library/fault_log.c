/**
 * @file    fault_log.c
 * @brief   故障与复位原因记录实现
 *
 * 写入顺序: 先写记录, 再写管理头; 掉电最多丢失正在写入的一条, 环形结构不损坏.
 * 管理头无独立 CRC, 通过 magic+version+范围校验识别; 记录带 CRC8.
 */

#include "fault_log.h"
#include "m24m01.h"
#include "gd32l23x.h"
#include "gd32l23x_rcu.h"
#include "gd32l23x_rtc.h"
#include "usart.h"
#include <string.h>

/* ==================== 存储布局 ==================== */
#define FAULT_LOG_BASE          0x0500U
#define FAULT_LOG_HEADER_SIZE   32U
#define FAULT_LOG_RECORD_BASE   (FAULT_LOG_BASE + FAULT_LOG_HEADER_SIZE)
#define FAULT_LOG_RECORD_SIZE   16U
#define FAULT_LOG_MAX_RECORDS   8U
#define FAULT_LOG_CRC_BYTES     13U     /* 记录前13字节参与CRC */

#define FAULT_LOG_MAGIC         0xF10AU
#define FAULT_LOG_VERSION       1U

/* ==================== 管理头 (32B) ==================== */
typedef struct {
    uint16_t magic;
    uint8_t  version;
    uint8_t  count;         /* 有效记录数 0~8 */
    uint8_t  head;          /* 下一写入槽位 0~7 */
    uint8_t  reserved[27];
} FaultLogHeader_t;

static FaultLogHeader_t s_hdr;
static uint8_t  s_inited   = 0;
static uint8_t  s_captured = 0;
static ResetCause_t s_reset_cause = RESET_CAUSE_UNKNOWN;

/* ==================== CRC8 (多项式 0x07, 初值 0x00) ==================== */
static uint8_t fault_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    uint16_t i;
    uint8_t j;

    for(i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(j = 0; j < 8; j++)
        {
            if(crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ==================== 复位原因捕获 ==================== */
void fault_log_capture_reset_cause(void)
{
    if(s_captured)
        return;

    if(rcu_flag_get(RCU_FLAG_SWRST) == SET)
        s_reset_cause = RESET_CAUSE_SOFT;
    else if(rcu_flag_get(RCU_FLAG_FWDGTRST) == SET)
        s_reset_cause = RESET_CAUSE_FWDGT;
    else if(rcu_flag_get(RCU_FLAG_WWDGTRST) == SET)
        s_reset_cause = RESET_CAUSE_WWDGT;
    else if(rcu_flag_get(RCU_FLAG_LPRST) == SET)
        s_reset_cause = RESET_CAUSE_LP;
    else if(rcu_flag_get(RCU_FLAG_EPRST) == SET)
        s_reset_cause = RESET_CAUSE_PIN;
    else if(rcu_flag_get(RCU_FLAG_V11RST) == SET)
        s_reset_cause = RESET_CAUSE_V11;
    else if(rcu_flag_get(RCU_FLAG_PORRST) == SET)
        s_reset_cause = RESET_CAUSE_POWER_ON;
    else
        s_reset_cause = RESET_CAUSE_UNKNOWN;

    s_captured = 1;
}

ResetCause_t fault_log_reset_cause(void)
{
    return s_reset_cause;
}

const char* fault_log_reset_cause_str(ResetCause_t cause)
{
    switch(cause)
    {
        case RESET_CAUSE_POWER_ON: return "POWER_ON";
        case RESET_CAUSE_PIN:      return "PIN";
        case RESET_CAUSE_SOFT:     return "SOFT";
        case RESET_CAUSE_FWDGT:    return "FWDGT";
        case RESET_CAUSE_WWDGT:    return "WWDGT";
        case RESET_CAUSE_LP:       return "LP";
        case RESET_CAUSE_V11:      return "V11";
        default:                   return "UNKNOWN";
    }
}

/* ==================== 时间戳 (RTC 字段本身为 BCD) ==================== */
static void fault_fill_time(FaultRecord_t *rec)
{
    rtc_parameter_struct t;

    memset(&t, 0, sizeof(t));
    rtc_current_time_get(&t);
    rec->year   = t.year;
    rec->month  = t.month;
    rec->date   = t.date;
    rec->hour   = t.hour;
    rec->minute = t.minute;
    rec->second = t.second;
}

/* ==================== 管理头读写 ==================== */
static void header_save(void)
{
    s_hdr.magic   = FAULT_LOG_MAGIC;
    s_hdr.version = FAULT_LOG_VERSION;
    M24M01_Write(FAULT_LOG_BASE, (const uint8_t *)&s_hdr, sizeof(FaultLogHeader_t));
}

static uint8_t header_load(void)
{
    FaultLogHeader_t tmp;

    M24M01_Read(FAULT_LOG_BASE, (uint8_t *)&tmp, sizeof(FaultLogHeader_t));
    if(tmp.magic != FAULT_LOG_MAGIC || tmp.version != FAULT_LOG_VERSION)
        return 1;
    if(tmp.count > FAULT_LOG_MAX_RECORDS || tmp.head >= FAULT_LOG_MAX_RECORDS)
        return 2;
    s_hdr = tmp;
    return 0;
}

static void fault_format(void)
{
    memset(&s_hdr, 0, sizeof(s_hdr));
    s_hdr.magic   = FAULT_LOG_MAGIC;
    s_hdr.version = FAULT_LOG_VERSION;
    header_save();
}

/* ==================== 接口实现 ==================== */
void fault_log_init(void)
{
    if(!s_captured)
        fault_log_capture_reset_cause();   /* 兜底: 未提前捕获则此刻读取 */

    if(header_load() != 0)
        fault_format();

    s_inited = 1;

    /* 记录本次复位 (含正常上电, 便于追溯启动历史) */
    fault_log_add(FAULT_CODE_RESET, (uint32_t)s_reset_cause);
    printf_debug("[FL] boot reset cause=%s records=%d\r\n",
                 fault_log_reset_cause_str(s_reset_cause), fault_log_count());
}

uint8_t fault_log_add(FaultCode_t code, uint32_t param)
{
    FaultRecord_t rec;

    if(!s_inited)
        return 1;

    memset(&rec, 0, sizeof(rec));
    rec.code        = (uint8_t)code;
    rec.reset_cause = (uint8_t)s_reset_cause;
    fault_fill_time(&rec);
    rec.param = param;
    rec.crc8  = fault_crc8((const uint8_t *)&rec, FAULT_LOG_CRC_BYTES);

    /* 先写记录, 再更新管理头 (掉电一致性) */
    M24M01_Write(FAULT_LOG_RECORD_BASE + (uint32_t)s_hdr.head * FAULT_LOG_RECORD_SIZE,
                 (const uint8_t *)&rec, sizeof(rec));

    s_hdr.head = (uint8_t)((s_hdr.head + 1) % FAULT_LOG_MAX_RECORDS);
    if(s_hdr.count < FAULT_LOG_MAX_RECORDS)
        s_hdr.count++;
    header_save();

    printf_debug("[FL] fault add code=0x%02X param=%lu\r\n",
                 (unsigned int)code, (unsigned long)param);
    return 0;
}

uint16_t fault_log_count(void)
{
    return s_inited ? s_hdr.count : 0;
}

uint8_t fault_log_read(uint16_t index, FaultRecord_t *out)
{
    FaultRecord_t rec;
    uint16_t slot;

    if(!s_inited || out == NULL || index >= s_hdr.count)
        return 1;

    /* 最旧在前 */
    slot = (uint16_t)((s_hdr.head + FAULT_LOG_MAX_RECORDS - s_hdr.count + index) % FAULT_LOG_MAX_RECORDS);
    M24M01_Read(FAULT_LOG_RECORD_BASE + (uint32_t)slot * FAULT_LOG_RECORD_SIZE,
                (uint8_t *)&rec, sizeof(rec));

    if(rec.crc8 != fault_crc8((const uint8_t *)&rec, FAULT_LOG_CRC_BYTES))
        return 2;

    *out = rec;
    return 0;
}

void fault_log_clear(void)
{
    fault_format();
}
