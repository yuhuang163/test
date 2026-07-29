
cmake_minimum_required(VERSION 3.24)

idf_component_register(
    SRCS "ls_meter.cpp" "main.cpp" "my_wifi.cpp" "uart.cpp" "ble.cpp" "ring_buf.cpp" "i2c.cpp" "adc.cpp"
    INCLUDE_DIRS "."
)
