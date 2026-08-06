/**
 * @file eeprom_history.c
 * @brief EEPROM 历史数据环形缓冲实现 (M24M01, 128KB)
 *
 * 存储区: 0x1000 ~ 0x1FFFF
 *   管理头 (8份 x 32B @0x1000, 占1页): magic(2)+version(1)+copy_idx(1)+seq(4)+head(2)+tail(2)+count(2)+crc8(1)+reserved(17)
 *   数据区 (126720B @0x1100): 3960条 x 32B
 *
 * 磨损均衡: 每次保存管理头写入下一个副本 (seq递增), 启动时选择 seq 最大的有效副本.
 * 掉电一致性: 先写记录数据, 再写管理头; 中途掉电最多丢失正在写入的一条, 环形结构不损坏.
 * 环形覆盖策略: 写满后 head 自动前进, 覆盖最旧记录.
 */

#include "eeprom_history.h"
#include "m24m01.h"
#include "tim.h"
#include "sensor_buffer.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

// ==================== 管理头结构 (32字节, 与记录大小对齐) ====================
typedef struct {
    uint16_t magic;          // EE_HIST_MAGIC
    uint8_t  version;        // EE_HIST_VERSION
    uint8_t  copy_idx;       // 副本序号 0~7
    uint32_t seq;            // 单调递增写入序号 (选择最新副本)
    uint16_t head;           // 写指针 (下一写入slot, 0~3959)
    uint16_t tail;           // 读指针 (最旧记录slot)
    uint16_t count;          // 有效记录数 (0~3960)
    uint8_t  crc8;           // 前14字节的CRC8
    uint8_t  reserved[17];   // 填充到32字节
} EeHistHeader_t;

// ==================== RAM 缓存管理头 (避免频繁读EEPROM) ====================
static EeHistHeader_t s_hdr;
static uint8_t s_inited = 0;

// ==================== CRC8 (多项式 0x07, 初值 0x00) ====================
static uint8_t crc8_calc(const uint8_t *data, uint16_t len)
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
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// ==================== 内部: 写管理头 (轮换副本, 磨损均衡) ====================
static void header_save(void)
{
    s_hdr.seq++;
    s_hdr.copy_idx = (uint8_t)(s_hdr.seq % EE_HIST_HEADER_COPIES);
    s_hdr.crc8 = crc8_calc((const uint8_t *)&s_hdr, 14);
    M24M01_Write(EE_HIST_HEADER_BASE + (uint32_t)s_hdr.copy_idx * EE_HIST_RECORD_SIZE,
                 (const uint8_t *)&s_hdr, sizeof(EeHistHeader_t));
}

// ==================== 内部: 从EEPROM读管理头 (选择最新有效副本) ====================
static uint8_t header_load(void)
{
    uint8_t  i;
    uint8_t  best_idx = 0xFF;
    uint32_t best_seq = 0;
    EeHistHeader_t tmp;

    for(i = 0; i < EE_HIST_HEADER_COPIES; i++)
    {
        M24M01_Read(EE_HIST_HEADER_BASE + (uint32_t)i * EE_HIST_RECORD_SIZE,
                    (uint8_t *)&tmp, sizeof(EeHistHeader_t));

        /* 校验 magic + 版本 + CRC */
        if(tmp.magic != EE_HIST_MAGIC || tmp.version != EE_HIST_VERSION)
            continue;
        if(tmp.crc8 != crc8_calc((const uint8_t *)&tmp, 14))
            continue;

        /* 选择写入序号最大的副本 */
        if(best_idx == 0xFF || tmp.seq > best_seq)
        {
            best_idx = i;
            best_seq = tmp.seq;
        }
    }

    if(best_idx == 0xFF)
        return 1;   /* 无有效副本 */

    s_hdr = tmp;

    /* 范围校验 */
    if(s_hdr.head >= EE_HIST_MAX_RECORDS ||
       s_hdr.tail >= EE_HIST_MAX_RECORDS ||
       s_hdr.count > EE_HIST_MAX_RECORDS)
        return 2;

    return 0;
}

// ==================== 内部: 格式化 (清空) 历史区 ====================
static void history_format(void)
{
    uint8_t i;

    memset(&s_hdr, 0, sizeof(s_hdr));
    s_hdr.magic    = EE_HIST_MAGIC;
    s_hdr.version  = EE_HIST_VERSION;
    s_hdr.head     = 0;
    s_hdr.tail     = 0;
    s_hdr.count    = 0;

    /* 8份副本全部写入全新状态 (seq=0~7), 保证任意副本可恢复 */
    for(i = 0; i < EE_HIST_HEADER_COPIES; i++)
    {
        s_hdr.seq      = i;
        s_hdr.copy_idx = i;
        s_hdr.crc8     = crc8_calc((const uint8_t *)&s_hdr, 14);
        M24M01_Write(EE_HIST_HEADER_BASE + (uint32_t)i * EE_HIST_RECORD_SIZE,
                     (const uint8_t *)&s_hdr, sizeof(EeHistHeader_t));
    }
}

// ==================== 内部: 获取BCD时间戳 ====================
static void get_bcd_timestamp(uint8_t *ts_bcd)
{
    rtc_parameter_struct tp;
    rtc_current_time_get(&tp);
    ts_bcd[0] = (uint8_t)(tp.year   & 0xFF);  // YY (BCD)
    ts_bcd[1] = (uint8_t)(tp.month  & 0xFF);  // MM (BCD)
    ts_bcd[2] = (uint8_t)(tp.date   & 0xFF);  // DD (BCD)
    ts_bcd[3] = (uint8_t)(tp.hour   & 0xFF);  // HH (BCD)
    ts_bcd[4] = (uint8_t)(tp.minute & 0xFF);  // MM (BCD)
    ts_bcd[5] = (uint8_t)(tp.second & 0xFF);  // SS (BCD)
}

// ==================== 内部: 计算记录EEPROM地址 (注意: 必须 uint32) ====================
static uint32_t record_addr(uint16_t slot)
{
    return EE_HIST_DATA_ADDR + (uint32_t)slot * EE_HIST_RECORD_SIZE;
}

// ==================== 内部: 轻量边界修复 (上电恢复后调用) ====================
static void boundary_repair(void)
{
    EeHistRecord_t rec;
    uint16_t guard = 0;
    uint8_t saved_crc;

    /* 校验最旧记录 (tail); CRC 损坏则跳过, 最多连续跳过 4 条 */
    while(s_hdr.count > 0 && guard < 4)
    {
        if(M24M01_Read(record_addr(s_hdr.tail), (uint8_t *)&rec, EE_HIST_RECORD_SIZE) != 0)
            break;
        saved_crc = rec.crc8;
        rec.crc8 = 0;
        if(crc8_calc((const uint8_t *)&rec, EE_HIST_RECORD_SIZE - 1) == saved_crc)
            break;
        s_hdr.tail = (uint16_t)((s_hdr.tail + 1) % EE_HIST_MAX_RECORDS);
        s_hdr.count--;
        guard++;
    }

    /* 校验最新记录 (head 前一条); 损坏则回退 head, 下次写入覆盖 */
    if(s_hdr.count > 0)
    {
        uint16_t last_slot = (uint16_t)((s_hdr.head + EE_HIST_MAX_RECORDS - 1) % EE_HIST_MAX_RECORDS);
        if(M24M01_Read(record_addr(last_slot), (uint8_t *)&rec, EE_HIST_RECORD_SIZE) == 0)
        {
            saved_crc = rec.crc8;
            rec.crc8 = 0;
            if(crc8_calc((const uint8_t *)&rec, EE_HIST_RECORD_SIZE - 1) != saved_crc)
            {
                s_hdr.head = last_slot;
                s_hdr.count--;
                guard++;
            }
        }
    }

    if(guard > 0)
        header_save();   /* 剔除过损坏记录, 落盘新状态 */
}

// ==================== 公开接口 ====================

void ee_hist_init(void)
{
    uint8_t st = header_load();

    if(st == 0)
    {
        boundary_repair();
        printf_debug("[EE_HIST] Restored: count=%d head=%d tail=%d seq=%lu\r\n",
                     (int)s_hdr.count, (int)s_hdr.head, (int)s_hdr.tail,
                     (unsigned long)s_hdr.seq);
    }
    else
    {
        printf_debug("[EE_HIST] No valid header (ret=%d), formatting...\r\n", (int)st);
        history_format();
    }
    s_inited = 1;
}

void ee_hist_push(float pressure, float temperature,
                  float pressure2, float temperature2,
                  float voltage, uint8_t flags)
{
    EeHistRecord_t rec;

    if(!s_inited) return;

    /* 填充记录 */
    rec.pressure = pressure;
    rec.temperature = temperature;
    rec.pressure2 = pressure2;
    rec.temperature2 = temperature2;
    rec.voltage = voltage;
    get_bcd_timestamp(rec.ts_bcd);
    rec.flags = flags;
    memset(rec.reserved, 0, sizeof(rec.reserved));

    /* 计算CRC8 (前31字节) */
    rec.crc8 = 0;
    rec.crc8 = crc8_calc((const uint8_t *)&rec, EE_HIST_RECORD_SIZE - 1);

    /* 先写记录数据, 后更新管理头 (掉电最多丢这一条) */
    M24M01_Write(record_addr(s_hdr.head), (const uint8_t *)&rec, EE_HIST_RECORD_SIZE);

    /* 更新环形指针 */
    s_hdr.head = (uint16_t)((s_hdr.head + 1) % EE_HIST_MAX_RECORDS);
    if(s_hdr.count < EE_HIST_MAX_RECORDS)
    {
        s_hdr.count++;
    }
    else
    {
        /* 满了, tail跟随前进 (覆盖最旧) */
        s_hdr.tail = (uint16_t)((s_hdr.tail + 1) % EE_HIST_MAX_RECORDS);
    }

    header_save();
}

uint8_t ee_hist_read(uint16_t index, EeHistRecord_t *out)
{
    if(!s_inited || !out) return 1;
    if(index >= s_hdr.count) return 1;

    /* 计算实际slot */
    uint16_t slot = (uint16_t)((s_hdr.tail + index) % EE_HIST_MAX_RECORDS);

    M24M01_Read(record_addr(slot), (uint8_t *)out, EE_HIST_RECORD_SIZE);

    /* CRC校验 */
    uint8_t saved_crc = out->crc8;
    out->crc8 = 0;
    uint8_t calc_crc = crc8_calc((const uint8_t *)out, EE_HIST_RECORD_SIZE - 1);
    out->crc8 = saved_crc;

    if(saved_crc != calc_crc)
        return 2;  /* CRC失败 */

    return 0;
}

uint16_t ee_hist_count(void)
{
    return s_inited ? s_hdr.count : 0;
}

void ee_hist_pop(void)
{
    if(!s_inited || s_hdr.count == 0) return;

    s_hdr.tail = (uint16_t)((s_hdr.tail + 1) % EE_HIST_MAX_RECORDS);
    s_hdr.count--;
    header_save();
}

void ee_hist_clear(void)
{
    if(!s_inited) return;
    history_format();
    printf_debug("[EE_HIST] Cleared\r\n");
}

void ee_hist_ts_to_str(const uint8_t *ts_bcd, char *buf)
{
    snprintf(buf, 20, "20%02x-%02x-%02x %02x:%02x:%02x",
             ts_bcd[0], ts_bcd[1], ts_bcd[2],
             ts_bcd[3], ts_bcd[4], ts_bcd[5]);
}

void ee_hist_to_sensor_record(const EeHistRecord_t *hist, void *out)
{
    SensorRecord_t *rec = (SensorRecord_t *)out;
    if(!hist || !rec) return;

    rec->pressure = hist->pressure;
    rec->temperature = hist->temperature;
    rec->pressure2 = hist->pressure2;
    rec->temperature2 = hist->temperature2;
    rec->voltage = hist->voltage;
    rec->flags = hist->flags;
    memset(rec->reserved, 0, sizeof(rec->reserved));

    /* BCD时间戳转字符串 */
    ee_hist_ts_to_str(hist->ts_bcd, rec->timestamp);
}
