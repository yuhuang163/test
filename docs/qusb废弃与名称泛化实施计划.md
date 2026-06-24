# agreement 协议层重构 - 第六阶段实施计划 (废弃 qusb 与名称泛化)

根据 `agreement按外设域拆分重构方案.md`，我们即将执行最后一阶段的重构。本阶段的目标是彻底废弃过渡性的 `agreement/qusb/` 目录，并消除协议层中关于 `ammeter` (电流表) 的历史遗留硬编码命名。

## Proposed Changes

### 1. 废弃 `agreement/qusb` 目录
在之前的重构中，`qusb` 中具体的设备编解码逻辑已经被下沉到了 `modbus/device` 与 `scpi/device`，现在的 `Qusb` 类仅仅是一个转发请求的空壳子。
- [DELETE] [qusb.h](file:///f:/C/test/agreement/qusb/qusb.h)
- [DELETE] [qusb.cpp](file:///f:/C/test/agreement/qusb/qusb.cpp)
- [DELETE] [qusb_types.h](file:///f:/C/test/agreement/qusb/qusb_types.h)

### 2. 将路由逻辑合并入 `test_base`
我们将 `Qusb` 中的路由分配逻辑（如 `Auto` / `Scpi` / `HqModbus` / `LxModbus`）移动到 `test_base` 中，使得工站能够直接控制自己的 USB 串口通道。
- [MODIFY] [test_base.h](file:///f:/C/test/work_station/test_base.h)
  - 移除对 `Qusb` 的依赖。
  - 新增 `QScpiManager scpiUsbManager_;` 成员以替代 `usb->scpiManager()`。
  - 增加枚举 `UsbProtocolRoute`（替代原来的 `QusbProtocolRoute`），以及 `setUsbProtocolConfig` 方法。
- [MODIFY] [test_base.cpp](file:///f:/C/test/work_station/test_base.cpp)
  - 移除实例化 `usb = new Qusb(...)` 的代码。
  - 在 `onUsbSerialFrame` 槽函数中，根据当前的路由模式判断是将数据丢给 `scpiUsbManager_.feedRx` 还是直接由 `modbusManager` 消化。
  - 对外提供兼容之前 `usb->setProtocolConfig()` 与 `usb->sendPowerInstruction()` 行为的内部函数封装，确保业务侧无需改动。

### 3. SCPI 协议层的命名泛化
根据规范 4.6 与 6.2，协议层不能再使用具体的外设名称（如 `ammeter` 电流表）。我们将泛化相关的名称。
- [MODIFY] [qscpimanager.h](file:///f:/C/test/agreement/scpi/manager/qscpimanager.h) & `qscpivisasession.h` & `qscpiserialsession.h`
  - 将信号 `ammeterReadingReceived(const QString&)` 改名为 `measureReadingReceived(const QString&)`。
- [MODIFY] [qprotocol_types.h](file:///f:/C/test/agreement/factory_protocol/access/qprotocol_types.h)
  - 将 `ProtocolAmmeterReadingData` 结构体重命名为通用的 `ProtocolMeasureData`（因为测量的不一定是电流，也可能是电压或吸力等传感器读数）。
- [MODIFY] `test_base.cpp` 及下游调用的地方（如 `suction.cpp` 等）
  - 配合更新信号连接和结构体名称。

### 4. 工程配置更新
- [MODIFY] [new_production.pro](file:///f:/C/test/new_production.pro)
  - 移除 `INCLUDEPATH += agreement/qusb`。
  - 从 `HEADERS` 和 `SOURCES` 中删除 `qusb` 相关文件。

## Verification Plan

### Automated Tests
- 本项目无自动化单元测试，我们将依靠编译阶段来进行验证。

### Manual Verification
- 进行 `qmake` 以及项目全量编译，确保所有的接口名称更换和文件移除不会导致 `Undefined Reference` 或编译报错。
- 如果条件允许，您可以打开一个使用到 USB 串口的工站（例如 Pico 吸力测试、带电流表的压感测试），确保它们能正常下发指令并接收数据。
