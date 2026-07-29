#pragma once

#include <Arduino.h>

// 低量程电流/电压表（Modbus RTU，Serial2）
class LSMeter {
public:
    static void read_and_print();
};
