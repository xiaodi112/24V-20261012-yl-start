# Host 单元测试

## 目的

在 PC 上编译并验证与芯片无关的业务逻辑, 无需硬件即可回归:

| 模块 | 覆盖内容 |
|---|---|
| `valve_fsm` | 开/关阀、超时、双限位预检、反向限位故障、压紧、抢占、失电恢复、忙时拒绝 |
| `threshold` | 压力/温度/电压阈值、滞回、突变、VPROTECT |
| `json_builder` | 字段拼接、缓冲上限安全 |
| `sensor_buffer` | 环形缓冲、覆盖、pop、越界 |

## 运行

Linux/macOS (CI 使用):

```sh
cd Tests
sh run_tests.sh
```

Windows:

```bat
cd Tests
run_tests.bat
```

## 设计说明

- `stubs/` 提供芯片头文件桩与运行时桩 (bsp_motor/bsp_wdt/EEPROM/fault_log/config/printf/时间),
  测试通过 `test_*` 全局控制限位、时间与电机状态, 从而驱动状态机。
- 被测模块源码直接取自 `my_Library`, 不做复制, 保证测试对象就是交付对象。
