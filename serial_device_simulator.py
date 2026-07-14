#!/usr/bin/env python3
import serial
import socket
import struct
import threading
import time
import random
import argparse
from datetime import datetime

class Asd9026aChannel:
    def __init__(self, addr):
        self.addr = addr
        self.output_enabled = True
        # 设置寄存器用 uV / mA
        self.battery_voltage_uv = 12000000
        self.max_current_ma = 2500
        self.current_range = 4
        self.display_speed = 2
        self.internal_resistance_mohm = 10
        self.protection_current_ma = 2000
        self.protection_power_w = 200
        self.protection_time_ms = 100
        # 温度编码: 实际℃ = 数值 - 40，65 → 25℃
        self.temperature = 65
        self.protection_flags = 0
        self.locked = False
        # 测量值与单位字段一致: 12V / 2500mA≈2.5A
        self.voltage_avg = 12
        self.current_avg = 2500
        self.voltage_unit = 1
        self.current_unit = 2
        self.current_direction = 0

    def set_output(self, enabled):
        self.output_enabled = enabled
        if enabled:
            self.voltage_avg = max(1, self.battery_voltage_uv // 1000000)
            self.current_avg = self.max_current_ma
        else:
            self.voltage_avg = 0
            self.current_avg = 0

    def update_measurements(self):
        if self.output_enabled:
            base_v = max(1, self.battery_voltage_uv // 1000000)
            self.voltage_avg = base_v
            self.current_avg = max(0, self.max_current_ma + int((time.time() * 50) % 20) - 10)

class SerialDeviceSimulator:
    def __init__(self, mode='serial', port='COM1', baud_rate=9600, tcp_host='0.0.0.0', tcp_port=5028):
        self.mode = mode
        self.port = port
        self.baud_rate = baud_rate
        self.tcp_host = tcp_host
        self.tcp_port = tcp_port
        self.ser = None
        self.server_socket = None
        self.running = False
        self.data_buffer = b""
        self.text_buffer = ""
        
        self.asd_channels = {
            0x01: Asd9026aChannel(0x01),
            0x02: Asd9026aChannel(0x02),
        }
        
        self.voltage = 12.0
        self.current = 0.5
        self.output_enabled = False

    def _log(self, direction, data):
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        if isinstance(data, str):
            hex_data = ' '.join(f'{ord(c):02X}' for c in data)
            print(f"[{timestamp}] {direction}: '{data}' | HEX: {hex_data}")
        else:
            hex_str = ' '.join(f'{b:02X}' for b in data)
            try:
                text_str = data.decode('utf-8', errors='ignore').replace('\n', '\\n').replace('\r', '\\r')
            except:
                text_str = ''
            if text_str:
                print(f"[{timestamp}] {direction}: '{text_str}' | HEX: {hex_str}")
            else:
                print(f"[{timestamp}] {direction}: HEX: {hex_str}")

    def _calculate_crc(self, data):
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

    def _build_asd_frame(self, addr, cmd, func, payload=b'', length=None):
        """回: 地址 + 命令码 + 功能码 + 数据长度 + 数据内容(高位在前) + 校验码"""
        payload = bytes(payload)
        if length is None:
            length = len(payload)
        body = bytes([addr, cmd, func, length & 0xFF]) + payload
        return body + struct.pack('<H', self._calculate_crc(body))

    def _is_modbus_frame(self, data):
        if len(data) < 6:
            return False
        fc = data[1]
        if 0x10 <= fc <= 0x22 or fc == 0x11 or fc == 0x21:
            crc = struct.unpack('<H', data[-2:])[0]
            calculated = self._calculate_crc(data[:-2])
            result = crc == calculated
            if result:
                print(f"检测到Modbus帧: 长度={len(data)}, FC=0x{fc:02X}")
            return result
        return False

    def _handle_modbus(self, data):
        if len(data) < 5:
            return None

        # 收: 地址 + 命令码 + 功能码 + 数据长度 [+ 数据内容] + 校验码
        addr = data[0]
        cmd = data[1]
        func = data[2] if len(data) > 2 else 0
        length = data[3] if len(data) > 3 else 0
        crc = struct.unpack('<H', data[-2:])[0]
        calculated_crc = self._calculate_crc(data[:-2])

        if crc != calculated_crc:
            print(f"CRC校验失败: 收到=0x{crc:04X}, 计算=0x{calculated_crc:04X}")
            return None

        print(f"ASD请求: 地址=0x{addr:02X}, 命令码=0x{cmd:02X}, 功能码=0x{func:02X}, 数据长度=0x{length:02X}")

        channel = self.asd_channels.get(addr)
        if not channel:
            return None

        response = None

        if cmd in (0x10, 0x20, 0x22):
            response = self._handle_modbus_read(channel, cmd, func, length)

        elif cmd in (0x11, 0x21):
            write_data = data[4:4 + length] if length else b''
            if not write_data and len(data) > 6:
                write_data = data[4:-2]
            response = self._handle_modbus_write(channel, cmd, func, write_data)

        elif cmd == 0x12:
            crc_out = self._calculate_crc(data[:-2])
            payload = struct.pack('<H', crc_out) + bytes([cmd])
            response = self._build_asd_frame(addr, cmd, 0x03, payload)

        if response:
            self._log("OUT", response)
        return response

    def _handle_modbus_read(self, channel, cmd, func, length):
        channel.update_measurements()

        if cmd == 0x10:
            if func == 0x00 and length == 0x02:
                data = bytearray(24)
                data[0] = 0x01
                data[1] = 0x02
                data[2] = 0x01
                data[5] = 0x55
                data[6] = 0x01
                data[7] = 0x02
                return self._build_asd_frame(channel.addr, cmd, func, data)

            elif func == 0x02 and length == 0x02:
                data = bytearray()
                data.append(channel.temperature & 0xFF)
                data.extend(struct.pack('>H', 12000))
                data.append(channel.protection_flags & 0xFF)
                data.append(0x00)
                data.append(0x00)
                return self._build_asd_frame(channel.addr, cmd, func, data)

        elif cmd == 0x20:
            # 读模拟电池设置: 请求 01 20 10 00 CRC → 回 01 20 10 0D + 13字节
            if func in (0x10, 0x0D):
                data = bytearray()
                data.append(1 if channel.output_enabled else 0)
                data.extend(struct.pack('>I', channel.battery_voltage_uv))
                data.extend(struct.pack('>I', channel.max_current_ma))
                data.append(channel.current_range)
                data.append(channel.display_speed)
                data.extend(struct.pack('>H', channel.internal_resistance_mohm))
                return self._build_asd_frame(channel.addr, cmd, 0x10, data)

        elif cmd == 0x22:
            # 读测量: 请求 01 22 10 00 CRC → 回 01 22 10 0E + 14字节
            if func == 0x10:
                data = bytearray()
                data.append(1 if channel.output_enabled else 0)
                data.extend(struct.pack('>I', channel.voltage_avg & 0xFFFFFFFF))
                data.append(channel.voltage_unit & 0xFF)
                data.extend(struct.pack('>I', abs(channel.current_avg) & 0xFFFFFFFF))
                data.append(channel.current_unit & 0xFF)
                data.append(channel.current_direction & 0xFF)
                data.append(channel.temperature & 0xFF)
                data.append(channel.protection_flags & 0xFF)
                return self._build_asd_frame(channel.addr, cmd, func, data)

        return None

    def _handle_modbus_write(self, channel, cmd, func, data):
        if cmd == 0x11:
            if func == 0x03 and len(data) >= 6:
                channel.current_range = data[0]
                channel.protection_current_ma = struct.unpack('>H', data[1:3])[0]
                channel.protection_power_w = struct.unpack('>H', data[3:5])[0]
                channel.protection_time_ms = data[5]
                print(f"通道{channel.addr} 保护设置: 量程={channel.current_range}, 保护电流={channel.protection_current_ma}mA")
                return self._build_asd_frame(channel.addr, cmd, func, length=0x03)

            elif func == 0x04 and len(data) >= 1:
                enable = data[0]
                channel.set_output(enable == 1)
                print(f"通道{channel.addr} 开关控制: {'开' if enable == 1 else '关'}")
                return self._build_asd_frame(channel.addr, cmd, func, length=0x03)

            elif func == 0x0C and len(data) >= 1:
                channel.locked = data[0] == 1
                print(f"通道{channel.addr} 面板锁屏: {'锁屏' if channel.locked else '解锁'}")
                return self._build_asd_frame(channel.addr, cmd, func, length=0x01)

        elif cmd == 0x21:
            if func == 0x10 and len(data) >= 13:
                if data[0] != 0:
                    channel.set_output(data[0] == 1)
                if struct.unpack('>I', data[1:5])[0] != 0:
                    channel.battery_voltage_uv = struct.unpack('>I', data[1:5])[0]
                if struct.unpack('>I', data[5:9])[0] != 0:
                    channel.max_current_ma = struct.unpack('>I', data[5:9])[0]
                if data[9] != 0:
                    channel.current_range = data[9]
                if data[10] != 0:
                    channel.display_speed = data[10]
                if struct.unpack('>H', data[11:13])[0] != 0:
                    channel.internal_resistance_mohm = struct.unpack('>H', data[11:13])[0]
                print(f"通道{channel.addr} 模拟电池设置: 电压={channel.battery_voltage_uv/1e6}V, 电流={channel.max_current_ma}mA")
                return self._build_asd_frame(channel.addr, cmd, func, length=0x0D)

        return None

    def _handle_text_command(self, command):
        command = command.strip()
        if not command:
            return ""

        self._log("IN", command)

        if command.startswith('AT'):
            return self._handle_at_command(command)

        if command == 'readonce':
            ch1_volt = 12.222
            ch1_curr = 2403.75
            ch1_power = ch1_volt * ch1_curr
            ch2_volt = 12.204
            ch2_curr = 2406.25
            ch2_power = ch2_volt * ch2_curr
            response = f"CH1:        {ch1_volt:.3f}V        {ch1_curr:.2f}mA        {ch1_power:.2f}mW\nCH2:        {ch2_volt:.3f}V        {ch2_curr:.2f}mA        {ch2_power:.2f}mW"
            self._log("OUT", response.replace('\n', '\\n'))
            return response

        if command.startswith('*IDN?'):
            response = "SERIAL_SIM,V1.0"
            self._log("OUT", response)
            return response

        if command.startswith('MEAS?'):
            noise_v = (random.random() - 0.5) * 0.1
            noise_c = (random.random() - 0.5) * 0.01
            v = self.voltage + noise_v
            c = self.current + noise_c
            response = f"VOLT:{v:.2f}V CURR:{c:.2f}A"
            self._log("OUT", response)
            return response

        if command.startswith('READ'):
            response = f"DATA:{random.randint(10000, 99999)}"
            self._log("OUT", response)
            return response

        if command.startswith('WRITE='):
            data = command[6:]
            print(f"Write data: {data}")
            response = "OK"
            self._log("OUT", response)
            return response

        if command.startswith('SET VOLT='):
            try:
                self.voltage = float(command[9:])
                print(f"Set voltage to {self.voltage}V")
                response = "OK"
                self._log("OUT", response)
                return response
            except ValueError:
                response = "ERROR"
                self._log("OUT", response)
                return response

        if command.startswith('SET CURR='):
            try:
                self.current = float(command[9:])
                print(f"Set current to {self.current}A")
                response = "OK"
                self._log("OUT", response)
                return response
            except ValueError:
                response = "ERROR"
                self._log("OUT", response)
                return response

        if command.startswith('OUTPUT ON'):
            self.output_enabled = True
            print("Output turned ON")
            response = "OK"
            self._log("OUT", response)
            return response

        if command.startswith('OUTPUT OFF'):
            self.output_enabled = False
            print("Output turned OFF")
            response = "OK"
            self._log("OUT", response)
            return response

        response = "ERROR: Unknown command"
        self._log("OUT", response)
        return response

    def _handle_at_command(self, command):
        if command == 'AT':
            response = "OK"
            self._log("OUT", response)
            return response

        if command == 'AT+VERSION?':
            response = "VER1.0"
            self._log("OUT", response)
            return response

        if command == 'AT+STATUS?':
            response = "OK"
            self._log("OUT", response)
            return response

        if command == 'AT+READ':
            response = f"DATA:{random.randint(10000, 99999)}"
            self._log("OUT", response)
            return response

        if command.startswith('AT+WRITE='):
            data = command[10:]
            print(f"AT Write: {data}")
            response = "OK"
            self._log("OUT", response)
            return response

        if command == 'AT+GMAC':
            response = "DF:A1:10:00:17:27"
            self._log("OUT", response)
            return response

        if command == 'AT+GMI':
            response = "BYD"
            self._log("OUT", response)
            return response

        if command == 'AT+GMM':
            response = "V3Pro"
            self._log("OUT", response)
            return response

        if command == 'AT+GMR':
            response = "1.0.0"
            self._log("OUT", response)
            return response

        response = "ERROR"
        self._log("OUT", response)
        return response

    def _process_data(self, data, send_func):
        self.data_buffer += data
        last_char_time = time.time()

        while True:
            if self._is_modbus_frame(self.data_buffer):
                response = self._handle_modbus(self.data_buffer)
                if response:
                    send_func(response)
                self.data_buffer = b""
                self.text_buffer = ""
            elif b'\n' in self.data_buffer or b'\r' in self.data_buffer:
                try:
                    text_data = self.data_buffer.decode('utf-8', errors='ignore')
                    self.text_buffer += text_data
                    self.data_buffer = b""
                    
                    while '\n' in self.text_buffer or '\r' in self.text_buffer:
                        if '\n' in self.text_buffer:
                            line, self.text_buffer = self.text_buffer.split('\n', 1)
                        elif '\r' in self.text_buffer:
                            line, self.text_buffer = self.text_buffer.split('\r', 1)
                        else:
                            break
                        line = line.strip()
                        if line:
                            response = self._handle_text_command(line)
                            if response:
                                send_func((response + '\r\n').encode('utf-8'))
                except:
                    self.data_buffer = b""
            elif time.time() - last_char_time > 0.5 and len(self.data_buffer) > 0:
                try:
                    text_data = self.data_buffer.decode('utf-8', errors='ignore')
                    self.text_buffer += text_data
                    if self.text_buffer.strip():
                        response = self._handle_text_command(self.text_buffer)
                        if response:
                            send_func((response + '\r\n').encode('utf-8'))
                except:
                    pass
                self.data_buffer = b""
                self.text_buffer = ""
                break
            else:
                break

    def _run_serial(self):
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud_rate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1
            )
            print(f"串口设备模拟器启动")
            print(f"模式: 串口")
            print(f"端口: {self.port}")
            print(f"波特率: {self.baud_rate}")
            print("支持: 文本命令 | Modbus RTU")
            print("等待连接...")

            while self.running:
                try:
                    if self.ser.in_waiting > 0:
                        data = self.ser.read(self.ser.in_waiting)
                        timestamp = time.strftime('%H:%M:%S.%f')[:-3]
                        hex_str = ' '.join(f'{b:02X}' for b in data)
                        print(f"[{timestamp}] 收到原始数据: {len(data)} bytes | HEX: {hex_str}")
                        self._process_data(data, self.ser.write)
                    time.sleep(0.01)
                except serial.SerialException as e:
                    print(f"串口错误: {e}")
                    break
                except Exception as e:
                    print(f"错误: {e}")

        except serial.SerialException as e:
            print(f"无法打开串口 {self.port}: {e}")
            print("请尝试使用TCP模式: python serial_device_simulator.py --mode tcp")
        finally:
            if self.ser and self.ser.is_open:
                self.ser.close()

    def _tcp_client_handler(self, conn, addr):
        print(f"TCP客户端连接: {addr}")
        conn.settimeout(5)
        try:
            while self.running:
                try:
                    data = conn.recv(1024)
                    if not data:
                        break
                    self._process_data(data, lambda x: conn.sendall(x))
                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"TCP客户端错误: {e}")
                    break
        finally:
            conn.close()
            print(f"TCP客户端断开: {addr}")

    def _run_tcp(self):
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.tcp_host, self.tcp_port))
            self.server_socket.listen(5)
            self.server_socket.settimeout(1.0)
            
            print(f"TCP设备模拟器启动")
            print(f"模式: TCP")
            print(f"地址: {self.tcp_host}:{self.tcp_port}")
            print("支持: 文本命令 | Modbus RTU")
            print("等待连接...")

            while self.running:
                try:
                    conn, addr = self.server_socket.accept()
                    threading.Thread(target=self._tcp_client_handler, args=(conn, addr), daemon=True).start()
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"TCP服务器错误: {e}")

        except Exception as e:
            print(f"无法启动TCP服务器: {e}")
        finally:
            if self.server_socket:
                self.server_socket.close()

    def run(self):
        self.running = True
        if self.mode == 'tcp':
            self._run_tcp()
        else:
            self._run_serial()
        print("设备模拟器停止")

    def stop(self):
        self.running = False

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='串口/TCP设备模拟器（支持文本命令和Modbus RTU）')
    parser.add_argument('--mode', type=str, default='serial', choices=['serial', 'tcp'], help='运行模式 (serial/tcp, 默认: serial)')
    parser.add_argument('--port', type=str, default='COM2', help='串口端口 (默认: COM2)')
    parser.add_argument('--baud', type=int, default=9600, help='波特率 (默认: 9600)')
    parser.add_argument('--tcp-host', type=str, default='0.0.0.0', help='TCP绑定地址 (默认: 0.0.0.0)')
    parser.add_argument('--tcp-port', type=int, default=5028, help='TCP端口 (默认: 5028)')
    args = parser.parse_args()

    simulator = SerialDeviceSimulator(
        mode=args.mode,
        port=args.port,
        baud_rate=args.baud,
        tcp_host=args.tcp_host,
        tcp_port=args.tcp_port
    )

    try:
        simulator.run()
    except KeyboardInterrupt:
        print("\n收到中断信号")
        simulator.stop()