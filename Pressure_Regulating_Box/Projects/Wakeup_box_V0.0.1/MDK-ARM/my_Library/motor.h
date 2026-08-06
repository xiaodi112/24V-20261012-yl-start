#ifndef __MOTOR_H
#define __MOTOR_H

#include "gd32l23x.h"

/* ============================================================
 * 电机驱动方式选择（编译宏开关, 仿 EEPROM_DRV_* 写法）
 *   MOTOR_DRV_GPIO : 高低电平直驱 (PA6/PA7 GPIO 输出)
 *   MOTOR_DRV_PWM  : PWM 驱动 (TIMER2_CH0→PA6 / CH1→PA7, 160kHz)
 * 必须且只能启用一个
 * ============================================================ */
#define MOTOR_DRV_GPIO
//#define MOTOR_DRV_PWM

#if defined(MOTOR_DRV_GPIO) && defined(MOTOR_DRV_PWM)
  #error "MOTOR: 只能选择一种驱动方式 (MOTOR_DRV_GPIO / MOTOR_DRV_PWM)"
#elif !defined(MOTOR_DRV_GPIO) && !defined(MOTOR_DRV_PWM)
  #error "MOTOR: 未选择驱动方式, 请在 motor.h 启用 MOTOR_DRV_GPIO 或 MOTOR_DRV_PWM"
#endif

#define MOTOR_PWM_FREQ_HZ   160000  /* PWM 驱动频率 */

void pwm_init(uint32_t freq, float duty_ch0, float duty_ch1);
void pwm_set_freq(uint32_t freq);
void pwm_set_duty(uint8_t ch, float duty);

#endif


