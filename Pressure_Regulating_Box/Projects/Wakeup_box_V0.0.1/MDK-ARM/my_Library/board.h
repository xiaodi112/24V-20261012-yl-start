/**
 * @file    board.h
 * @brief   板级引脚定义 (全工程引脚编号的唯一来源)
 *
 * 换板/换引脚时只需修改本文件; 业务与驱动代码不得直接出现 GPIO 端口/引脚编号.
 * 注意: 早期文档将 USART0 记为 PA2/PA3, 当前工程实际配置为 PC4/PC5, 以本文件为准.
 */
#ifndef BOARD_H
#define BOARD_H

#include "gd32l23x.h"

/* ==================== 4G 模组 (EC800E/EC801E) ==================== */
#define BOARD_4G_V4G_EN_PORT       GPIOB
#define BOARD_4G_V4G_EN_PIN        GPIO_PIN_5     /* PB5: 4G 电源使能 HIGH=上电 */
#define BOARD_4G_DTR_PORT          GPIOA
#define BOARD_4G_DTR_PIN           GPIO_PIN_0     /* PA0: 4G DTR HIGH=允许休眠 */
#define BOARD_4G_RI_PORT           GPIOA
#define BOARD_4G_RI_PIN            GPIO_PIN_1     /* PA1: 4G RI 唤醒输入 */
#define BOARD_4G_LPUART_TX_PORT    GPIOD
#define BOARD_4G_LPUART_TX_PIN     GPIO_PIN_8     /* PD8: LPUART0_TX */
#define BOARD_4G_LPUART_RX_PORT    GPIOD
#define BOARD_4G_LPUART_RX_PIN     GPIO_PIN_9     /* PD9: LPUART0_RX */

/* ==================== 电机 (阀门执行机构) ==================== */
#define BOARD_MOTOR_SLP_PORT       GPIOA
#define BOARD_MOTOR_SLP_PIN        GPIO_PIN_5     /* PA5: M_SLP 驱动使能 */
#define BOARD_MOTOR_A_PORT         GPIOA
#define BOARD_MOTOR_A_PIN          GPIO_PIN_6     /* PA6: 电机 A */
#define BOARD_MOTOR_B_PORT         GPIOA
#define BOARD_MOTOR_B_PIN          GPIO_PIN_7     /* PA7: 电机 B */
#define BOARD_MOTOR_LIMIT_OPEN_PORT    GPIOA
#define BOARD_MOTOR_LIMIT_OPEN_PIN     GPIO_PIN_11 /* PA11: K1-S 开阀到位 LOW=到位 */
#define BOARD_MOTOR_LIMIT_CLOSE_PORT   GPIOA
#define BOARD_MOTOR_LIMIT_CLOSE_PIN    GPIO_PIN_12 /* PA12: K2-S 关阀到位 LOW=到位 */

/* ==================== BLE 模组 (VG6328A) ==================== */
#define BOARD_BLE_PWR_PORT         GPIOC
#define BOARD_BLE_PWR_PIN          GPIO_PIN_12    /* PC12: 蓝牙电源 HIGH=上电 */
#define BOARD_BLE_LINK_PORT        GPIOD
#define BOARD_BLE_LINK_PIN         GPIO_PIN_2     /* PD2: BLE 连接状态 LOW=已连接 */
#define BOARD_BLE_UART_TX_PORT     GPIOB
#define BOARD_BLE_UART_TX_PIN      GPIO_PIN_3     /* PB3: UART4_TX */
#define BOARD_BLE_UART_RX_PORT     GPIOB
#define BOARD_BLE_UART_RX_PIN      GPIO_PIN_4     /* PB4: UART4_RX */

/* ==================== 调试串口 USART0 ==================== */
#define BOARD_DBG_UART_TX_PORT     GPIOC
#define BOARD_DBG_UART_TX_PIN      GPIO_PIN_4     /* PC4: USART0_TX */
#define BOARD_DBG_UART_RX_PORT     GPIOC
#define BOARD_DBG_UART_RX_PIN      GPIO_PIN_5     /* PC5: USART0_RX */

/* ==================== 按键 / 指示 ==================== */
#define BOARD_BUTTON_PORT          GPIOD
#define BOARD_BUTTON_PIN           GPIO_PIN_0     /* PD0: Btn1 按下=LOW (EXTI0) */
#define BOARD_LED_PORT             GPIOA
#define BOARD_LED_PIN              GPIO_PIN_15    /* PA15: LED1 */
#define BOARD_BUZZER_PORT          GPIOB
#define BOARD_BUZZER_PIN           GPIO_PIN_8     /* PB8: BUZZER HIGH=响 */

/* ==================== I2C (位操作) ==================== */
#define BOARD_I2C0_SCL_PORT        GPIOC
#define BOARD_I2C0_SCL_PIN         GPIO_PIN_0     /* PC0: 传感器1/EEPROM 总线 SCL */
#define BOARD_I2C0_SDA_PORT        GPIOC
#define BOARD_I2C0_SDA_PIN         GPIO_PIN_1     /* PC1: 传感器1/EEPROM 总线 SDA */
#define BOARD_I2C1_SCL_PORT        GPIOB
#define BOARD_I2C1_SCL_PIN         GPIO_PIN_6     /* PB6: 传感器2 总线 SCL */
#define BOARD_I2C1_SDA_PORT        GPIOB
#define BOARD_I2C1_SDA_PIN         GPIO_PIN_7     /* PB7: 传感器2 总线 SDA */
#define BOARD_I2C_PWR_SW_PORT      GPIOD
#define BOARD_I2C_PWR_SW_PIN       GPIO_PIN_4     /* PD4: SW_IIC 电子开关 HIGH=上电 */

/* ==================== ADC ==================== */
#define BOARD_ADC_BAT_PORT         GPIOC
#define BOARD_ADC_BAT_PIN          GPIO_PIN_2     /* PC2: 电池电压 ADC */

#endif /* BOARD_H */
