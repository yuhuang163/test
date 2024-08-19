
cmake_minimum_required(VERSION 3.24)

idf_component_register(
    SRCS "main.cpp" "my_wifi.cpp" "uart.cpp" "ble.cpp" "ring_buf.cpp"
    INCLUDE_DIRS "."
)
