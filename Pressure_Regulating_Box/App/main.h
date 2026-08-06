#ifndef MAIN_H
#define MAIN_H

#include "stdint.h"
#include "sensor_ctx.h"
#include "ota_info.h"
#include "threshold.h"

#define APP_FW_VERSION_NUM     1

// 公共功能函数 (main.c 定义, downlink.c 等模块调用)
void lcd_shutdown(void);
const char* alarm_try_close_valve(ThresholdResult_t result);
// 定时阀门周期检查 (阻塞流程内调用): keep_online=1 保持4G在线不改变会话
void vtiming_check(uint8_t keep_online);

#endif
