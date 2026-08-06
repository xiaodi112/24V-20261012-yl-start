/**
 * @file    bsp_motor.c
 * @brief   电机驱动 BSP 实现 (GPIO 直驱 / PWM 预留)
 *
 * 引脚定义见 board.h, 驱动方式见 motor.h (MOTOR_DRV_GPIO / MOTOR_DRV_PWM).
 */

#include "bsp_motor.h"
#include "board.h"
#include "motor.h"
#include "gd32l23x.h"

uint8_t g_motor_enabled = 1;

void bsp_motor_init(void)
{
    if(!g_motor_enabled)
        return;

    /* 电机相关引脚全部位于 GPIOA (见 board.h), 统一使能时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);

    /* M_SLP: 驱动芯片使能, 推挽, 初始 LOW 关闭 */
    gpio_mode_set(BOARD_MOTOR_SLP_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, BOARD_MOTOR_SLP_PIN);
    gpio_output_options_set(BOARD_MOTOR_SLP_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, BOARD_MOTOR_SLP_PIN);
    gpio_bit_reset(BOARD_MOTOR_SLP_PORT, BOARD_MOTOR_SLP_PIN);

    /* 限位输入 (K1/K2 同端口, 内部上拉, 低电平=到位) */
    gpio_mode_set(BOARD_MOTOR_LIMIT_OPEN_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
                  BOARD_MOTOR_LIMIT_OPEN_PIN | BOARD_MOTOR_LIMIT_CLOSE_PIN);

#if defined(MOTOR_DRV_PWM)
    /* PWM 驱动: PA6/PA7 复用 TIMER2 CH0/CH1, 初始两路 0% 停止 */
    pwm_init(MOTOR_PWM_FREQ_HZ, 0.0f, 0.0f);
#else
    /* 高低电平直驱: 双低停止 */
    gpio_mode_set(BOARD_MOTOR_A_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  BOARD_MOTOR_A_PIN | BOARD_MOTOR_B_PIN);
    gpio_bit_reset(BOARD_MOTOR_A_PORT, BOARD_MOTOR_A_PIN);
    gpio_bit_reset(BOARD_MOTOR_B_PORT, BOARD_MOTOR_B_PIN);
#endif
}

uint8_t bsp_motor_limit_open(void)
{
    return (gpio_input_bit_get(BOARD_MOTOR_LIMIT_OPEN_PORT, BOARD_MOTOR_LIMIT_OPEN_PIN) == RESET) ? 1 : 0;
}

uint8_t bsp_motor_limit_close(void)
{
    return (gpio_input_bit_get(BOARD_MOTOR_LIMIT_CLOSE_PORT, BOARD_MOTOR_LIMIT_CLOSE_PIN) == RESET) ? 1 : 0;
}

void bsp_motor_enable(uint8_t on)
{
    if(on)
        gpio_bit_set(BOARD_MOTOR_SLP_PORT, BOARD_MOTOR_SLP_PIN);
    else
        gpio_bit_reset(BOARD_MOTOR_SLP_PORT, BOARD_MOTOR_SLP_PIN);
}

void bsp_motor_drive_open(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.0f);
    pwm_set_duty(1, 0.999f);
#else
    gpio_bit_reset(BOARD_MOTOR_A_PORT, BOARD_MOTOR_A_PIN);
    gpio_bit_set(BOARD_MOTOR_B_PORT, BOARD_MOTOR_B_PIN);
#endif
}

void bsp_motor_drive_close(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.999f);
    pwm_set_duty(1, 0.0f);
#else
    gpio_bit_set(BOARD_MOTOR_A_PORT, BOARD_MOTOR_A_PIN);
    gpio_bit_reset(BOARD_MOTOR_B_PORT, BOARD_MOTOR_B_PIN);
#endif
}

void bsp_motor_drive_stop(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.0f);
    pwm_set_duty(1, 0.0f);
#else
    gpio_bit_reset(BOARD_MOTOR_A_PORT, BOARD_MOTOR_A_PIN);
    gpio_bit_reset(BOARD_MOTOR_B_PORT, BOARD_MOTOR_B_PIN);
#endif
}
