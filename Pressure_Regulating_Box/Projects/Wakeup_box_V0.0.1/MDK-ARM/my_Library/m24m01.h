#ifndef M24M01__H
#define M24M01__H

#include "stdint.h"

/* ============================================================
 * EEPROM 芯片型号选择（编译宏开关, 仿 PT304D 驱动协议选择写法）
 * 使用方法: 启用对应芯片宏, 注释掉另一个; 必须且只能启用一个
 *   EEPROM_DRV_M24M01  : M24M01-RMN6P 128KB, 256B页, 17位地址(A16)
 *   EEPROM_DRV_AT24C64 : AT24C64 8KB, 32B页, 13位地址
 * ============================================================ */
#define EEPROM_DRV_M24M01
//#define EEPROM_DRV_AT24C64

#if defined(EEPROM_DRV_M24M01) && defined(EEPROM_DRV_AT24C64)
  #error "EEPROM: 只能选择一种芯片型号 (EEPROM_DRV_M24M01 / EEPROM_DRV_AT24C64)"
#elif defined(EEPROM_DRV_M24M01)
  /* ---- M24M01-RMN6P 关键参数（规格书）----
   * - 容量: 1Mbit = 128K Bytes (131072)
   * - 组织: 512页 x 256字节/页
   * - 17位地址: A16编入设备地址字节 bit1, A15~A0 为 2 字节字地址
   * - 设备地址字节: 1010 E2 E1 A16 R/W  (E2=E1=0 接地)
   * - 页写大小: 256字节
   * - 写周期: 最大 5ms */
  #define EEPROM_BASE_ADDR   0xA0    /* 基础设备地址 1010_00_0_0 (E2=0,E1=0) */
  #define EEPROM_PAGE_SIZE   256     /* 页大小 256字节 */
  #define EEPROM_TOTAL_SIZE  131072  /* 总容量 128KB */
  #define EEPROM_WRITE_TIME  5       /* 写周期 5ms */
  /* 根据17位地址计算设备地址字节（A16编入 bit1） */
  #define EEPROM_DEV_ADDR(full_addr)  (uint8_t)(EEPROM_BASE_ADDR | ((((uint32_t)(full_addr) >> 16) & 0x01) << 1))
  /* 取高字节字地址 A15~A8 */
  #define EEPROM_ADDR_HI(full_addr)   (uint8_t)(((full_addr) >> 8) & 0xFF)
#elif defined(EEPROM_DRV_AT24C64)
  /* ---- AT24C64 关键参数 ----
   * - 容量: 64Kbit = 8K Bytes (8192)
   * - 组织: 256页 x 32字节/页
   * - 13位地址: 2字节字地址 (高字节低5位有效, A12~A8)
   * - 设备地址字节: 1010 E2 E1 0 R/W (E2=E1=0 接地), 恒 0xA0
   * - 页写大小: 32字节
   * - 写周期: 最大 5ms */
  #define EEPROM_BASE_ADDR   0xA0    /* 基础设备地址 1010_00_0_0 (E2=0,E1=0) */
  #define EEPROM_PAGE_SIZE   32      /* 页大小 32字节 */
  #define EEPROM_TOTAL_SIZE  8192    /* 总容量 8KB */
  #define EEPROM_WRITE_TIME  5       /* 写周期 5ms */
  /* AT24C64 无 A16, 设备地址固定 0xA0 */
  #define EEPROM_DEV_ADDR(full_addr)  (EEPROM_BASE_ADDR)
  /* 取高字节字地址 A12~A8 (13位地址) */
  #define EEPROM_ADDR_HI(full_addr)   (uint8_t)(((full_addr) >> 8) & 0x1F)
#else
  #error "EEPROM: 未选择芯片型号, 请在 m24m01.h 启用 EEPROM_DRV_M24M01 或 EEPROM_DRV_AT24C64"
#endif

/* 取低字节字地址 A7~A0 (两种芯片一致) */
#define EEPROM_ADDR_LO(full_addr)   (uint8_t)((full_addr) & 0xFF)

/* ============================================================
 * 兼容旧宏名 (M24M01_*): 保持 m24m01.c 与外部调用方无需修改
 * ============================================================ */
#define M24M01_BASE_ADDR    EEPROM_BASE_ADDR
#define M24M01_PAGE_SIZE    EEPROM_PAGE_SIZE
#define M24M01_TOTAL_SIZE   EEPROM_TOTAL_SIZE
#define M24M01_WRITE_TIME   EEPROM_WRITE_TIME
#define M24M01_DEV_ADDR(full_addr)  EEPROM_DEV_ADDR(full_addr)
#define M24M01_ADDR_HI(full_addr)   EEPROM_ADDR_HI(full_addr)
#define M24M01_ADDR_LO(full_addr)   EEPROM_ADDR_LO(full_addr)

/* 写单字节 */
uint8_t M24M01_WriteByte(uint32_t addr, uint8_t data);
/* 页写（最大 256 字节，不可跨页） */
uint8_t M24M01_WritePage(uint32_t addr, const uint8_t *data, uint16_t len);
/* 任意长度写（自动分页，支持跨页） */
uint8_t M24M01_Write(uint32_t addr, const uint8_t *data, uint16_t len);
/* 随机读（任意地址，任意长度） */
uint8_t M24M01_Read(uint32_t addr, uint8_t *buf, uint16_t len);
/* ACK轮询等待写周期完成 */
uint8_t M24M01_WaitReady(uint32_t addr, uint16_t timeout_ms);
/* 读写测试（通过调试串口输出结果） */
void M24M01_Test(void);

#endif
