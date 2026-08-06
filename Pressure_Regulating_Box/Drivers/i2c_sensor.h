#ifndef I2C_SENSOR_H
#define I2C_SENSOR_H

#include "gd32l23x.h"
#include "board.h"

/* 位操作IIC连接的GPIO引脚 (定义见 board.h, 传感器1/EEPROM 总线) */
#define GPIO_PORT_IIC     BOARD_I2C0_SCL_PORT
#define RCC_IIC_ENABLE    rcu_periph_clock_enable(BOARD_I2C0_SCL_PORT == GPIOC ? RCU_GPIOC : RCU_GPIOB)
#define IIC_SCL_PIN       BOARD_I2C0_SCL_PIN
#define IIC_SDA_PIN       BOARD_I2C0_SDA_PIN

/* 读写SCL和SDA的宏，优先使用GPIO库函数实现（兼容GD32标准库） */
#define IIC_SCL_H()  gpio_bit_set(GPIO_PORT_IIC, IIC_SCL_PIN)        /* SCL = 1 */
#define IIC_SCL_L()  gpio_bit_reset(GPIO_PORT_IIC, IIC_SCL_PIN)      /* SCL = 0 */

#define IIC_SDA_H()  gpio_bit_set(GPIO_PORT_IIC, IIC_SDA_PIN)        /* SDA = 1 */
#define IIC_SDA_L()  gpio_bit_reset(GPIO_PORT_IIC, IIC_SDA_PIN)      /* SDA = 0 */

#define IIC_SDA_READ()  gpio_input_bit_get(GPIO_PORT_IIC, IIC_SDA_PIN) /* 读SDA引脚状态 */

/* IIC读写方向定义（增强代码可读性，使用 #ifndef 防止与 i2c_eeprom.h 重定义冲突） */
#ifndef IIC_WR
#define IIC_WR    0   /* 写操作 */
#define IIC_RD    1   /* 读操作 */
#endif

void IIC_Delay(void);
void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t IIC_Wait_Ack(int16_t timeout);
uint8_t IIC_Read_Byte(uint8_t ack);

#endif /* I2C_SENSOR_H */
