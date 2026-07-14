#!/usr/bin/env python3
import socket
import struct
import threading
import time
import logging

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class Asd9026aChannel:
    def __init__(self, addr):
        self.addr = addr
        self.output_enabled = False
        self.battery_voltage_uv = 3700000
        self.max_current_ma = 1000
        self.current_range = 4
        self.display_speed = 2
        self.internal_resistance_mohm = 10
        self.protection_current_ma = 2000
        self.protection_power_w = 200
        self.protection_time_ms = 100
        self.temperature = 65
        self.protection_flags = 0
        self.locked = False
        self.voltage_avg = 3700000
        self.current_avg = 0
        self.voltage_unit = 1
        self.current_unit = 2
        self.current_direction = 0

    def set_output(self, enabled):
        self.output_enabled = enabled
        if enabled:
            self.voltage_avg = self.battery_voltage_uv
            self.current_avg = 0
        else:
            self.voltage_avg = 0
            self.current_avg = 0

    def update_measurements(self):
        if self.output_enabled:
            self.voltage_avg = self.battery_voltage_uv + int((time.time() * 100) % 1000) - 500
            self.current_avg = int((time.time() * 50) % 100) - 50


class Asd9026aSimulator:
    def __init__(self, host='0.0.0.0', port=5027):
        self.host = host
        self.port = port
        self.channels = {
            0x01: Asd9026aChannel(0x01),
            0x02: Asd9026aChannel(0x02),
        }
        self.server_socket = None
        self.running = False

    def calculate_crc(self, data):
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc >>= 1
                    crc ^= 0xA001
                else:
                    crc >>= 1
        return crc

    def handle_modbus_read(self, addr, fc, start_addr, quantity):
        channel = self.channels.get(addr)
        if not channel:
            return None

        if fc == 0x10:
            if start_addr == 0x00 and quantity == 0x02:
                channel.update_measurements()
                data = bytearray()
                data.append(0x01)
                data.append(0x02)
                data.append(0x01)
                data.append(0x00)
                data.append(0x00)
                data.append(0x55)
                data.append(0x01)
                data.append(0x02)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                data.append(0x00)
                crc = self.calculate_crc([addr, fc, 0x18] + list(data))
                response = struct.pack('>BBB', addr, fc, 0x18) + data + struct.pack('<H', crc)
                return response

            elif start_addr == 0x02 and quantity == 0x02:
                channel.update_measurements()
                temp = channel.temperature
                input_voltage_mv = 12000
                flags = channel.protection_flags
                data = bytearray()
                data.append(temp & 0xFF)
                data.append((input_voltage_mv >> 8) & 0xFF)
                data.append(input_voltage_mv & 0xFF)
                data.append(flags & 0xFF)
                data.append(0x00)
                data.append(0x00)
                crc = self.calculate_crc([addr, fc, 0x06] + list(data))
                response = struct.pack('>BBB', addr, fc, 0x06) + data + struct.pack('<H', crc)
                return response

        elif fc == 0x20:
            if start_addr == 0x10 and quantity == 0x00:
                channel.update_measurements()
                data = bytearray()
                data.append(0x00)
                data.extend(struct.pack('<I', channel.battery_voltage_uv))
                data.extend(struct.pack('<I', channel.max_current_ma))
                data.append(channel.current_range)
                data.append(channel.display_speed)
                data.extend(struct.pack('<H', channel.internal_resistance_mohm))
                crc = self.calculate_crc([addr, fc, 0x0D] + list(data))
                response = struct.pack('>BBB', addr, fc, 0x0D) + data + struct.pack('<H', crc)
                return response

        elif fc == 0x22:
            if start_addr == 0x10 and quantity == 0x0E:
                channel.update_measurements()
                data = bytearray()
                data.append(1 if channel.output_enabled else 0)
                data.extend(struct.pack('<I', channel.voltage_avg))
                data.append(channel.voltage_unit)
                data.extend(struct.pack('<I', abs(channel.current_avg)))
                data.append(channel.current_unit)
                data.append(channel.current_direction)
                data.append(channel.temperature)
                data.append(channel.protection_flags)
                crc = self.calculate_crc([addr, fc, 0x0E] + list(data))
                response = struct.pack('>BBB', addr, fc, 0x0E) + data + struct.pack('<H', crc)
                return response

        return None

    def handle_modbus_write(self, addr, fc, start_addr, data):
        channel = self.channels.get(addr)
        if not channel:
            return None

        if fc == 0x11:
            if start_addr == 0x03:
                channel.current_range = data[0]
                channel.protection_current_ma = struct.unpack('<H', data[1:3])[0]
                channel.protection_power_w = struct.unpack('<H', data[3:5])[0]
                channel.protection_time_ms = data[5]
                logger.info(f"通道{addr} 保护设置: 量程={channel.current_range}, 保护电流={channel.protection_current_ma}mA, 保护功率={channel.protection_power_w}W")
                crc = self.calculate_crc([addr, fc, 0x03])
                response = struct.pack('>BBBH', addr, fc, 0x03, crc)
                return response

            elif start_addr == 0x04:
                enable = data[0]
                channel.set_output(enable == 1)
                logger.info(f"通道{addr} 开关控制: {'开' if enable == 1 else '关'}")
                crc = self.calculate_crc([addr, fc, 0x03])
                response = struct.pack('>BBBH', addr, fc, 0x03, crc)
                return response

            elif start_addr == 0x0C:
                channel.locked = data[0] == 1
                logger.info(f"通道{addr} 面板锁屏: {'锁屏' if channel.locked else '解锁'}")
                crc = self.calculate_crc([addr, fc, 0x01])
                response = struct.pack('>BBBH', addr, fc, 0x01, crc)
                return response

        elif fc == 0x21:
            if start_addr == 0x10:
                if data[0] != 0:
                    channel.set_output(data[0] == 1)
                if struct.unpack('<I', data[1:5])[0] != 0:
                    channel.battery_voltage_uv = struct.unpack('<I', data[1:5])[0]
                if struct.unpack('<I', data[5:9])[0] != 0:
                    channel.max_current_ma = struct.unpack('<I', data[5:9])[0]
                if data[9] != 0:
                    channel.current_range = data[9]
                if data[10] != 0:
                    channel.display_speed = data[10]
                if struct.unpack('<H', data[11:13])[0] != 0:
                    channel.internal_resistance_mohm = struct.unpack('<H', data[11:13])[0]
                logger.info(f"通道{addr} 模拟电池设置: 电压={channel.battery_voltage_uv/1e6}V, 电流={channel.max_current_ma}mA")
                crc = self.calculate_crc([addr, fc, 0x0D])
                response = struct.pack('>BBBH', addr, fc, 0x0D, crc)
                return response

        return None

    def handle_request(self, data):
        if len(data) < 8:
            return None

        addr = data[0]
        fc = data[1]

        logger.info(f"收到请求: 地址=0x{addr:02X}, 功能码=0x{fc:02X}, 数据={data.hex()}")

        if fc in (0x10, 0x20, 0x22):
            if len(data) >= 6:
                start_addr = struct.unpack('>H', data[2:4])[0]
                quantity = struct.unpack('>H', data[4:6])[0] if len(data) >= 6 else 0
                return self.handle_modbus_read(addr, fc, start_addr, quantity)

        elif fc == 0x11:
            if len(data) >= 6:
                start_addr = struct.unpack('>H', data[2:4])[0]
                byte_count = data[4] if len(data) > 4 else 0
                write_data = data[5:5+byte_count] if len(data) > 5+byte_count else data[5:]
                return self.handle_modbus_write(addr, fc, start_addr, write_data)

        elif fc == 0x21:
            if len(data) >= 6:
                start_addr = struct.unpack('>H', data[2:4])[0]
                byte_count = data[4] if len(data) > 4 else 0
                write_data = data[5:5+byte_count] if len(data) > 5+byte_count else data[5:]
                return self.handle_modbus_write(addr, fc, start_addr, write_data)

        elif fc == 0x12:
            crc = self.calculate_crc(data[:-2])
            received_crc = struct.unpack('<H', data[-2:])[0]
            data_out = bytearray()
            data_out.extend(struct.pack('<H', crc))
            data_out.append(fc)
            response_crc = self.calculate_crc([addr, fc, 0x03] + list(data_out))
            response = struct.pack('>BBB', addr, fc, 0x03) + data_out + struct.pack('<H', response_crc)
            return response

        return None

    def client_handler(self, conn, addr):
        logger.info(f"客户端连接: {addr}")
        try:
            while self.running:
                conn.settimeout(5)
                try:
                    data = conn.recv(1024)
                    if not data:
                        break
                    response = self.handle_request(data)
                    if response:
                        logger.info(f"发送响应: {response.hex()}")
                        conn.sendall(response)
                except socket.timeout:
                    continue
                except Exception as e:
                    logger.error(f"处理请求失败: {e}")
                    break
        finally:
            conn.close()
            logger.info(f"客户端断开: {addr}")

    def start(self):
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        self.running = True
        logger.info(f"ASD9026A 双通道模拟电池模拟器启动")
        logger.info(f"监听地址: {self.host}:{self.port}")
        logger.info(f"通道1地址: 0x01")
        logger.info(f"通道2地址: 0x02")

        while self.running:
            try:
                conn, addr = self.server_socket.accept()
                threading.Thread(target=self.client_handler, args=(conn, addr), daemon=True).start()
            except Exception as e:
                if self.running:
                    logger.error(f"接受连接失败: {e}")

    def stop(self):
        self.running = False
        if self.server_socket:
            self.server_socket.close()


if __name__ == "__main__":
    simulator = Asd9026aSimulator()
    try:
        simulator.start()
    except KeyboardInterrupt:
        logger.info("收到中断信号，停止模拟器")
        simulator.stop()