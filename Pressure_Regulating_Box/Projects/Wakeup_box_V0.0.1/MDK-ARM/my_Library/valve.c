#include "valve.h"
#include "motor.h"
#include "eeprom_config.h"
#include "delay.h"
#include "usart.h"
#include <string.h>

// 电机功能开关: 0=关闭(默认), 1=开启
uint8_t g_motor_enabled = 1;

// ==================== 电机 GPIO 初始化 ====================
void motor_init(void)
{
    if(!g_motor_enabled) return;
    rcu_periph_clock_enable(RCU_GPIOA);

    /* PA5: M_SLP 电机驱动使能输出, 推挽, 初始LOW关闭 */
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_5);
    gpio_bit_reset(GPIOA, GPIO_PIN_5);

    /* PA11: 限位1(开阀到位), PA12: 限位2(关阀到位), 内部上拉（低电平=到位） */
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_11 | GPIO_PIN_12);

#if defined(MOTOR_DRV_PWM)
    /* PWM 驱动: PA6/PA7 复用 TIMER2 CH0/CH1, 初始两路0%停止 */
    pwm_init(MOTOR_PWM_FREQ_HZ, 0.0f, 0.0f);
#else
    /* 高低电平直驱: PA6/PA7 GPIO 输出, 初始双低停止 */
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_bit_reset(GPIOA, GPIO_PIN_7);
#endif
}

// ==================== 限位开关读取 ====================
// PA11: K1-S 开阀到位（低电平=到位）
uint8_t motor_limit1_reached(void)
{
    return (gpio_input_bit_get(GPIOA, GPIO_PIN_11) == RESET) ? 1 : 0;
}

// PA12: K2-S 关阀到位（低电平=到位）
uint8_t motor_limit2_reached(void)
{
    return (gpio_input_bit_get(GPIOA, GPIO_PIN_12) == RESET) ? 1 : 0;
}

// ==================== 电机驱动原语 (宏选择驱动方式) ====================
// M_SLP: 驱动芯片使能, 动作期间置HIGH, 结束后置LOW (两种驱动方式共用)
static void motor_slp_set(uint8_t on)
{
    if(on)
        gpio_bit_set(GPIOA, GPIO_PIN_5);
    else
        gpio_bit_reset(GPIOA, GPIO_PIN_5);
}

// 开阀方向: PWM=CH0 0% / CH1 99.9%; GPIO=PA6低/PA7高
static void motor_drive_open(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.0f);
    pwm_set_duty(1, 0.999f);
#else
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_bit_set(GPIOA, GPIO_PIN_7);
#endif
}

// 关阀方向: PWM=CH0 99.9% / CH1 0%; GPIO=PA6高/PA7低
static void motor_drive_close(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.999f);
    pwm_set_duty(1, 0.0f);
#else
    gpio_bit_set(GPIOA, GPIO_PIN_6);
    gpio_bit_reset(GPIOA, GPIO_PIN_7);
#endif
}

// 停止: 两路全停 (GPIO双低 = PWM双0%输出低, 行为一致)
static void motor_drive_stop(void)
{
#if defined(MOTOR_DRV_PWM)
    pwm_set_duty(0, 0.0f);
    pwm_set_duty(1, 0.0f);
#else
    gpio_bit_reset(GPIOA, GPIO_PIN_6);
    gpio_bit_reset(GPIOA, GPIO_PIN_7);
#endif
}

// ==================== 阀门动作执行 (含valve_err检测) ====================
const char* valve_execute(const char *action, uint8_t update_last_order)
{
    if(!g_motor_enabled)
    {
        printf_debug("[VALVE] Motor disabled, skip\r\n");
        return "valve_err";
    }

    uint32_t t;
    uint8_t target_limit, wrong_limit;
    uint8_t is_open  = (strcmp(action, "open") == 0 || strcmp(action, "1") == 0);
    uint8_t is_close = (strcmp(action, "close") == 0 || strcmp(action, "0") == 0);

    if(!is_open && !is_close)
        return "valve_err";

    // M_SLP HIGH: 使能电机驱动 (两种驱动方式共用)
    motor_slp_set(1);

    if(is_open)
    {
        printf_debug("[SM] Valve OPEN\r\n");
        motor_drive_open();
        t = trea_millis();
        while(!motor_limit1_reached() && (trea_millis() - t) < 20000);

        // 到位后保持原动作 (delay_switch=1 时), 用于压紧阀芯
        // 安全 clamp: 即使 g_vdelay.delay_ms 被外部误修改, 也不超过 VDLY_DELAY_MS_MAX
        if(motor_limit1_reached() && g_vdelay.enabled && g_vdelay.delay_ms > 0)
        {
            uint16_t hold_ms = g_vdelay.delay_ms > VDLY_DELAY_MS_MAX ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
            printf_debug("[SM] Valve OPEN limit reached, hold %dms\r\n", hold_ms);
            trea_delay_ms(hold_ms);
        }

        motor_drive_stop();

        target_limit = motor_limit1_reached();
        wrong_limit  = motor_limit2_reached();
    }
    else
    {
        printf_debug("[SM] Valve CLOSE\r\n");
        motor_drive_close();
        t = trea_millis();
        while(!motor_limit2_reached() && (trea_millis() - t) < 20000);

        // 到位后保持原动作 (delay_switch=1 时), 用于压紧阀芯
        if(motor_limit2_reached() && g_vdelay.enabled && g_vdelay.delay_ms > 0)
        {
            uint16_t hold_ms = g_vdelay.delay_ms > VDLY_DELAY_MS_MAX ? VDLY_DELAY_MS_MAX : g_vdelay.delay_ms;
            printf_debug("[SM] Valve CLOSE limit reached, hold %dms\r\n", hold_ms);
            trea_delay_ms(hold_ms);
        }

        motor_drive_stop();

        target_limit = motor_limit2_reached();
        wrong_limit  = motor_limit1_reached();
    }

    // M_SLP LOW: 关闭电机驱动
    motor_slp_set(0);

    // valve_err: 两个限位同时触发, 或目标未到位且反向限位也未触发(机械卡死)
    if(target_limit && wrong_limit)
        return "valve_err";
    if(target_limit)
    {
        if(update_last_order) { g_valve_last_order = is_open ? 1 : 0; config_save_ext(); }
        return is_open ? "valve_on" : "valve_off";
    }
    if(wrong_limit)
        return "valve_err";
    return "valve_timeout";
}

// ==================== 阀门实时状态查询 (不动作) ====================
const char* valve_get_status(void)
{
    uint8_t l1 = motor_limit1_reached();  // 开阀到位
    uint8_t l2 = motor_limit2_reached();  // 关阀到位

    if(l1 && l2)  return "valve_err";
    if(l1)        return "valve_on";
    if(l2)        return "valve_off";
    return "valve_timeout";
}
