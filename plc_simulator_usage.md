# PLC按键控制模拟器使用说明

## 概述

这个Python模拟器模拟了村早PLC控制按键的逻辑，用于测试和开发按键测试功能。

## 核心功能

### 1. Modbus TCP服务器
- 实现标准的Modbus TCP协议
- 支持读取线圈（功能码0x01）
- 支持写入单个线圈（功能码0x05）
- 支持写入多个线圈（功能码0x0F）

### 2. PLC按键控制逻辑
- M线圈状态管理
- 位置就绪信号模拟
- 步骤完成信号模拟
- 按键完成信号模拟
- 电容值模拟

### 3. 按键测试流程
- 模拟按键按下动作
- 模拟按键完成信号
- 支持多个按键的序列测试

## 配置参数

```python
PlcConfig(
    host="192.168.1.88",        # PLC IP地址
    port=502,                   # Modbus TCP端口
    unit_id=1,                  # 从站ID
    m_base=200,                 # M线圈基地址
    m_coil_offset=0,            # M线圈地址偏移
    position_ready_base=250,    # 位置就绪信号基地址
    step_done_base=260,         # 步骤完成信号基地址
    key_done_offset=10          # 按键完成信号偏移
)
```

## 地址映射

| 功能 | 地址 | 说明 |
|------|------|------|
| 按键控制 | M200-M206 | 对应7个按键的控制信号 |
| 位置就绪 | M250 | 位置准备好信号 |
| 步骤完成 | M260 | 当前步骤完成信号 |
| 按键完成 | M210 | 按键动作完成信号 |

## 使用方法

### 启动模拟器

```bash
python plc_key_simulator.py
```

### 交互命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `status` | 显示当前状态 | `status` |
| `press <index>` | 模拟按键按下 | `press 0` |
| `setcap <kk> <value>` | 设置电容值 | `setcap 0 3500` |
| `sequence` | 运行完整按键测试序列 | `sequence` |
| `quit` | 退出模拟器 | `quit` |

### 编程接口

```python
from plc_key_simulator import PlcKeyTestSimulator, PlcConfig

# 创建配置
config = PlcConfig(
    host="192.168.1.88",
    port=502
)

# 创建模拟器
simulator = PlcKeyTestSimulator(config)

# 启动模拟
simulator.start_simulation()

# 模拟单个按键
simulator.simulate_key_press(key_index=0, delay_ms=100)

# 设置电容值
simulator.modbus_server.set_key_cap_value(kk=0, value=3500)

# 运行完整测试序列
simulator.simulate_key_test_sequence(
    key_indices=[0, 1, 2, 3, 4, 5, 6],
    key_caps={0: 3500, 1: 3200, 2: 3800}
)

# 获取状态
status = simulator.get_status()

# 停止模拟
simulator.stop_simulation()
```

## 与原C++代码的对应关系

### 原C++代码功能映射

| C++函数 | Python对应功能 |
|---------|----------------|
| `plcReadCoil()` | `ModbusTcpServer.get_coil()` |
| `plcWriteCoil()` | `ModbusTcpServer.set_coil()` |
| `plcWaitCoilTrue()` | 客户端轮询检查 |
| `plcWaitCoilFalse()` | 客户端轮询检查 |
| `runPlcV3TouchKeyFull()` | `simulate_key_press()` |
| `pollKeyCapDuringPress()` | `get_key_cap_value()` |

### 测试流程对应

1. **连接PLC**: 建立Modbus TCP连接
2. **检查位置就绪**: 读取M250状态
3. **发送按键指令**: 写入M200-M206对应按键
4. **等待按键完成**: 等待M210变为True
5. **读取电容值**: 通过协议读取按键电容
6. **发送步骤完成**: 写入M260
7. **释放按键**: 清除按键控制信号

## 日志输出

模拟器会输出详细的日志信息，包括：
- 服务器启动/停止
- 客户端连接/断开
- Modbus请求/响应
- 线圈状态变化
- 按键模拟过程

## 故障排除

### 连接问题
- 检查IP地址和端口配置
- 确保防火墙允许502端口
- 验证网络连接

### 状态不同步
- 检查M线圈地址映射
- 确认偏移量配置正确
- 查看日志了解详细状态

### 电容值异常
- 检查电容值范围设置
- 确认KK索引正确
- 验证客户端读取逻辑

## 扩展功能

可以根据需要扩展以下功能：
1. 添加更多Modbus功能码支持
2. 实现复杂的按键时序控制
3. 添加故障注入功能
4. 支持多个PLC设备模拟
5. 添加Web界面控制

## 注意事项

1. 确保Python环境已安装（推荐Python 3.7+）
2. 模拟器默认监听192.168.1.88:502，可根据需要修改
3. 电容值范围默认为[1, 65535]，可通过配置调整
4. 按键索引范围为0-6，对应7个按键
5. 模拟器使用后台线程运行，注意线程安全