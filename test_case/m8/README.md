# Qroot 测试用例

测试步骤 ini 中 `Protocol=Qroot`（大小写不敏感，与 `qroot` 等价）。

部署：将本目录下 `*.ini` 复制到运行目录 `test_case/`。

上位机全局协议：`SYSTEM/ProtocolType=qroot`。

## 用例列表

| 文件 | 指令 | CID |
|------|------|-----|
| M8_读取版本号.ini | SoftVersionRead | 0x91 |
| M8_获取电量信息.ini | GetBattery | 0xE0 |
| M8_查询电池温度.ini | RootBatteryTempQuery | 0x80 |
| M8_进入老化.ini | BurningMode | 0xAF |
| M8_恢复出厂.ini | FactoryReset | 0xFC |
| M8_开启按键上报.ini | ButtonState(1) | 0x9A |
| M8_关闭按键上报.ini | ButtonState(0) | 0x9A |
| M8_LED全开.ini | LedTest(on=1) | 0x93 |
| M8_LED全关.ini | LedTest(on=0) | 0x93 |
