#include "mygpio.h"
#include "board.h"

// ===================== 板级输出控制 (引脚见 board.h) =====================
// 4G 电源: PB5 HIGH=上电
void gpio_set_4g_power(uint8_t on)
{
    if(on)
        gpio_bit_set(BOARD_4G_V4G_EN_PORT, BOARD_4G_V4G_EN_PIN);
    else
        gpio_bit_reset(BOARD_4G_V4G_EN_PORT, BOARD_4G_V4G_EN_PIN);
}

// 4G 电源状态: 返回 1=上电
uint8_t gpio_get_4g_power(void)
{
    return (gpio_input_bit_get(BOARD_4G_V4G_EN_PORT, BOARD_4G_V4G_EN_PIN) == SET) ? 1 : 0;
}

// 蓝牙电源: PC12 HIGH=上电
void gpio_set_ble_power(uint8_t on)
{
    if(on)
        gpio_bit_set(BOARD_BLE_PWR_PORT, BOARD_BLE_PWR_PIN);
    else
        gpio_bit_reset(BOARD_BLE_PWR_PORT, BOARD_BLE_PWR_PIN);
}

// I2C 电子开关: PD4 HIGH=上电
void gpio_set_i2c_power(uint8_t on)
{
    /* 每次调用重新配置输出模式: 深睡唤醒后 PD4 可能被置为 Analog */
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(BOARD_I2C_PWR_SW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_I2C_PWR_SW_PIN);
    gpio_output_options_set(BOARD_I2C_PWR_SW_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_I2C_PWR_SW_PIN);
    if(on)
        gpio_bit_set(BOARD_I2C_PWR_SW_PORT, BOARD_I2C_PWR_SW_PIN);
    else
        gpio_bit_reset(BOARD_I2C_PWR_SW_PORT, BOARD_I2C_PWR_SW_PIN);
}

// 深睡前将调试串口引脚拉低 (含历史保留的 PA2/PA3)
void gpio_debug_uart_pins_low(void)
{
    gpio_bit_reset(BOARD_DBG_UART_TX_PORT, BOARD_DBG_UART_TX_PIN);
    gpio_bit_reset(BOARD_DBG_UART_RX_PORT, BOARD_DBG_UART_RX_PIN);
    gpio_bit_reset(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);   /* 历史保留: 文档旧记 USART0 */
}

void gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOF);

    /* 先写 ODR 再切输出模式, 避免模式切换瞬间的电平跳变:
     *   PA0(4G_DTR):  默认 HIGH (允许 EC800E 休眠)
     *   PA15(LED1):   默认 LOW (LED 默认息灭)
     *   PB8(BUZZER):  默认 LOW (蜂鸣器默认关闭)
     *   PC12(SW_BT 蓝牙开关): 默认 LOW (蓝牙默认断电)
     *   PD4(SW_IIC 电子开关): 默认 LOW (节能, 采集时按需上电)
     *   PB5(V4G_EN 4G 电源): 默认 LOW (4G 默认断电)
     */

    gpio_mode_set(BOARD_4G_DTR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_4G_DTR_PIN);
    gpio_output_options_set(BOARD_4G_DTR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_4G_DTR_PIN);
	gpio_bit_set(BOARD_4G_DTR_PORT, BOARD_4G_DTR_PIN);

    gpio_mode_set(BOARD_BUZZER_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_BUZZER_PIN);
    gpio_output_options_set(BOARD_BUZZER_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_BUZZER_PIN);

    gpio_mode_set(BOARD_BLE_PWR_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_BLE_PWR_PIN);
    gpio_output_options_set(BOARD_BLE_PWR_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_BLE_PWR_PIN);

    gpio_mode_set(BOARD_I2C_PWR_SW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_I2C_PWR_SW_PIN);
    gpio_output_options_set(BOARD_I2C_PWR_SW_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_I2C_PWR_SW_PIN);

    gpio_mode_set(BOARD_4G_V4G_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_4G_V4G_EN_PIN);
    gpio_output_options_set(BOARD_4G_V4G_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_4G_V4G_EN_PIN);
}

// ===================== PD0(Btn1) 按键唤醒 =====================
// 深休眠前调用: PD0输入+上拉 → EXTI0下降沿 → NVIC使能
void button_exti_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SYSCFG);

    // PD0(Btn1): 输入, 内部上拉 (空闲HIGH, 按下LOW)
    gpio_mode_set(GPIOD, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);

    // EXTI0 → PD0(Btn1), 下降沿触发唤醒
    syscfg_exti_line_config(EXTI_SOURCE_GPIOD, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_flag_clear(EXTI_0);

    // NVIC使能 EXTI0 独立中断
    nvic_irq_enable(EXTI0_IRQn, 2);

}

// 唤醒后调用: 禁用EXTI0 + 清标志, 防止抖动重复触发
void button_exti_disable(void)
{
    exti_interrupt_disable(EXTI_0);
    exti_flag_clear(EXTI_0);
    nvic_irq_disable(EXTI0_IRQn);
}

// 读取PD0(Btn1)电平: 1=按下(LOW), 0=松开(HIGH)
uint8_t button_is_pressed(void)
{
    return (gpio_input_bit_get(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN) == RESET) ? 1 : 0;
}

// ===================== 控制 4G 模组 DTR 引脚 (PA0) =====================
// level: 1 = HIGH (允许模块休眠); 0 = LOW (强制唤醒)
void gpio_set_4g_dtr(uint8_t level)
{
    if(level)
    {
        gpio_bit_set(BOARD_4G_DTR_PORT, BOARD_4G_DTR_PIN);
    }
    else
    {
        gpio_bit_reset(BOARD_4G_DTR_PORT, BOARD_4G_DTR_PIN);
    }
}

// ===================== PD2 蓝牙连接状态引脚 =====================
// BLE模块(VG6328A-MS)连接成功后输出LOW, 未连接时HIGH
void ble_link_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    gpio_mode_set(BOARD_BLE_LINK_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_BLE_LINK_PIN);
}

uint8_t ble_link_is_connected(void)
{
    return (gpio_input_bit_get(BOARD_BLE_LINK_PORT, BOARD_BLE_LINK_PIN) == RESET) ? 1 : 0;
}

// ===================== PD2 蓝牙连接 EXTI 唤醒 =====================
// BLE开启时深休眠前调用: PD2 EXTI2 下降沿唤醒
void ble_exti_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_SYSCFG);

    // PD2: 输入, 内部上拉 (空闲HIGH, 连接LOW)
    gpio_mode_set(BOARD_BLE_LINK_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_BLE_LINK_PIN);

    // EXTI2 → PD2, 下降沿触发
    syscfg_exti_line_config(EXTI_SOURCE_GPIOD, EXTI_SOURCE_PIN2);
    exti_init(EXTI_2, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_flag_clear(EXTI_2);

    // GD32L23x: EXTI2独立中断
    nvic_irq_enable(EXTI2_IRQn, 2);

}

void ble_exti_disable(void)
{
    exti_interrupt_disable(EXTI_2);
    exti_flag_clear(EXTI_2);
    nvic_irq_disable(EXTI2_IRQn);
}

// ===================== PA1 4G RI引脚 EXTI 唤醒 =====================
// EC800E 收到服务器下行数据时 RI 脚产生低电平脉冲(~120ms)
// PA1 下降沿中断唤醒 MCU, 与 LPUART 起始位唤醒互为冗余
void ri_exti_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);

    // PA1: 输入, 内部上拉 (RI空闲HIGH, 有数据时LOW)
    gpio_mode_set(BOARD_4G_RI_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, BOARD_4G_RI_PIN);

    // EXTI1 → PA1, 下降沿触发
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN1);
    exti_init(EXTI_1, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_flag_clear(EXTI_1);

    nvic_irq_enable(EXTI1_IRQn, 1);

}

void ri_exti_disable(void)
{
    exti_interrupt_disable(EXTI_1);
    exti_flag_clear(EXTI_1);
    nvic_irq_disable(EXTI1_IRQn);
}
