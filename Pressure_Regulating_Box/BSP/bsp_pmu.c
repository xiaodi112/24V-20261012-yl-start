/**
 * @file    bsp_pmu.c
 * @brief   板级电源管理 BSP 实现
 *
 * 深休眠策略 (现状, 不改): 活跃阶段软件看门狗兜底, 深睡期间关闭;
 * 唤醒源: LPUART 起始位 / RTC 闹钟 / 按键 / 4G RI / BLE 连接.
 */

#include "bsp_pmu.h"
#include "board.h"
#include "usart.h"
#include "tim.h"
#include "mygpio.h"
#include "delay.h"
#include "systick.h"
#include "sched.h"
#include "gd32l23x.h"
#include "gd32l23x_pmu.h"

/* ===================== 时钟恢复 ===================== */
static void bsp_clock_reconfig(void)
{
    RCU_CTL |= RCU_CTL_IRC16MEN;
    while(rcu_osci_stab_wait(RCU_IRC16M) != SUCCESS);

    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV1);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);

    RCU_CTL &= ~RCU_CTL_PLLEN;
    RCU_CFG0 &= ~RCU_CFG0_SCS;
    RCU_CFG0 |= RCU_CKSYSSRC_IRC16M;
}

/* ===================== 深睡前 GPIO 低功耗配置 ===================== */
void bsp_pmu_low_power_pins(uint8_t lpuart_enabled, uint8_t lcd_active, uint8_t bt_on)
{
    /* 1. 确保所有GPIO时钟开启以允许配置其寄存器 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOF);

    /* 2. GPIOA: 排除 PA1 (4G RI 唤醒) / PA8-10 (SLCD COM0-2, LCD 开启时);
     * PA5 (M_SLP 电机驱动使能) 保持输出 LOW; PA0 (4G DTR) 保持输出 HIGH (允许模块休眠, 浮空会被内部下拉拉低导致不休眠) */
    uint32_t gpioa_analog_pins = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
                                 GPIO_PIN_6 | GPIO_PIN_7 |
                                 GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15;
    if(!lpuart_enabled)
        gpioa_analog_pins |= GPIO_PIN_1;   /* PA1: 4G 断电时转 Analog */
    if(!lcd_active)
        gpioa_analog_pins |= (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, gpioa_analog_pins);

    /* 3. GPIOB: 排除 SLCD (LCD 开启时) / PB3-PB4 (蓝牙开启时) */
    uint32_t gpiob_analog_pins = GPIO_PIN_2 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    if(!bt_on)
        gpiob_analog_pins |= (GPIO_PIN_3 | GPIO_PIN_4);
    if(!lcd_active)
        gpiob_analog_pins |= (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_9 |
                              GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                              GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, gpiob_analog_pins);

    /* 4. GPIOC: 排除 SLCD SEG (LCD 开启时) / PC12 (蓝牙电源) / PC14-15 (LXTAL, 绝对不动) */
    uint32_t gpioc_analog_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                                 GPIO_PIN_4 | GPIO_PIN_5 |
                                 GPIO_PIN_13;
    if(!lcd_active)
    {
        gpioc_analog_pins |= (GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 |
                              GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11);
        rcu_periph_clock_disable(RCU_SLCD);
    }
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, gpioc_analog_pins);

    /* 5. GPIOD: PD0 (按键唤醒) 保持输入; PD4 (I2C 电源开关) 保持输出 LOW (转 Analog 会使 MOSFET 栅极浮空误导通); PD8/9 (LPUART) 按需保留 */
    uint32_t gpiod_analog_pins = GPIO_PIN_3 |
                                 GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                                 GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                                 GPIO_PIN_14 | GPIO_PIN_15;
    if(!bt_on)
        gpiod_analog_pins |= GPIO_PIN_2;   /* PD2: BLE 连接状态 */
    if(!lpuart_enabled)
        gpiod_analog_pins |= (GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOD, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, gpiod_analog_pins);

    /* 关键电源/使能脚深睡期显式定电平 (不转 Analog, 防止浮空误导通/误唤醒):
     *   PD4: I2C 传感器电源开关 (低=断电); PA5: M_SLP 电机使能 (低=关闭); PA0: 4G DTR (高=允许模块休眠) */
    gpio_bit_reset(GPIOD, GPIO_PIN_4);
    gpio_bit_reset(GPIOA, GPIO_PIN_5);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);   /* PA0: 4G DTR 输出模式 */
    gpio_bit_set(GPIOA, GPIO_PIN_0);                                /* PA0: DTR HIGH = 允许 4G 模块休眠 */
    /* 6. GPIOF 全部未使用, 转 Analog */
    gpio_mode_set(GPIOF, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0 | GPIO_PIN_1);
}

/* ===================== 进入深休眠 ===================== */
void bsp_pmu_enter_deepsleep(uint8_t lpuart_enabled, uint8_t lcd_active, uint8_t bt_on)
{
    /* 清空历史事件, 避免陈旧唤醒源导致立即返回 */
    sched_event_get();

    /* 软件看门狗: 进入深休眠前必须关闭 (深休眠期间 SysTick 停摆, 否则唤醒后计数错乱)
     * 所有进入深休眠的路径都经过本函数, 此处统一关闭可保证休眠期不被误复位 */
    swdt_disable();

    /* 配置未使用 GPIO 转 ANALOG 以极致降低漏电 */
    bsp_pmu_low_power_pins(lpuart_enabled, lcd_active, bt_on);

    rcu_periph_clock_enable(RCU_PMU);
    pmu_to_deepsleepmode(PMU_LDNPDSP_LOWDRIVE, WFI_CMD, PMU_DEEPSLEEP);

    /* 等待任意唤醒事件 (只读不消费, 由调用方快照处理) */
    while(sched_event_pending() == 0);
}

/* ===================== 唤醒后系统重新初始化 ===================== */
void bsp_pmu_wakeup_reinit(uint8_t lpuart_woke)
{
    /* 恢复时钟 + 滴答定时器 + TIMER1（必须最先执行） */
    bsp_clock_reconfig();
    systick_config();
    my_systick_config();
    timer1_delay_init(); /* 深休眠后 APB1 停摆, 必须重新启动 TIMER1 计数器 */

    /* 无论哪种唤醒源，都必须关闭LPUART唤醒模式和中断 + RI EXTI
     * 否则LPUART唤醒中断持续触发，无法再次进入深休眠 */
    lpuart_wakeup_disable(LPUART0);
    lpuart_interrupt_disable(LPUART0, LPUART_INT_WU);
    exti_flag_clear(EXTI_28);

    /* 关闭RI引脚EXTI (防止唤醒后RI持续低电平重复触发) */
    ri_exti_disable();

    /* LPUART唤醒时: 消费触发唤醒的字节 (深休眠期间DMA未运行, 字节在RDATA中) */
    if(lpuart_woke)
    {
        if(RESET != lpuart_flag_get(LPUART0, LPUART_FLAG_RBNE))
            lpuart_data_receive(LPUART0);
        lpuart_receive_config(LPUART0, LPUART_RECEIVE_ENABLE);
        while(RESET == lpuart_flag_get(LPUART0, LPUART_FLAG_TC));
        lpuart_disable(LPUART0);
    }
}
