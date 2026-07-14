#!/usr/bin/env python3
import sys
import os
import time
import threading
import struct
import random

try:
    import win32file
    import win32pipe
    import win32api
    import win32con
    HAS_PYWIN32 = True
except ImportError:
    HAS_PYWIN32 = False

try:
    import serial
    HAS_PYSERIAL = True
except ImportError:
    HAS_PYSERIAL = False


class VirtualSerialPort:
    def __init__(self, port_name):
        self.port_name = port_name
        self.pipe_name = f"\\\\.\\pipe\\{port_name}"
        self.pipe_handle = None
        self.client_handle = None
        self.running = False
        self.read_buffer = b""
        
    def create(self):
        try:
            self.pipe_handle = win32pipe.CreateNamedPipe(
                self.pipe_name,
                win32pipe.PIPE_ACCESS_DUPLEX,
                win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_READMODE_BYTE | win32pipe.PIPE_WAIT,
                1,
                65536,
                65536,
                0,
                None
            )
            print(f"虚拟串口管道创建: {self.pipe_name}")
            return True
        except Exception as e:
            print(f"创建管道失败: {e}")
            return False
            
    def wait_for_connection(self):
        try:
            win32pipe.ConnectNamedPipe(self.pipe_handle, None)
            print(f"客户端已连接到虚拟串口 {self.port_name}")
            self.running = True
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
            
    def read(self, size=1024):
        try:
            if not self.running:
                return b""
                
            while len(self.read_buffer) < size:
                data = win32file.ReadFile(self.pipe_handle, 1024)[1]
                if not data:
                    break
                self.read_buffer += data
                
            result = self.read_buffer[:size]
            self.read_buffer = self.read_buffer[size:]
            return result
        except Exception as e:
            return b""
            
    def write(self, data):
        try:
            if self.running:
                win32file.WriteFile(self.pipe_handle, data)
                return len(data)
            return 0
        except Exception as e:
            return 0
            
    def close(self):
        self.running = False
        if self.client_handle:
            win32file.CloseHandle(self.client_handle)
        if self.pipe_handle:
            win32file.CloseHandle(self.pipe_handle)
            win32pipe.DisconnectNamedPipe(self.pipe_handle)
        print(f"虚拟串口 {self.port_name} 已关闭")


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


class VirtualSerialSimulator:
    def __init__(self, sim_port='COM2', app_port='COM3', baud_rate=9600):
        self.sim_port = sim_port
        self.app_port = app_port
        self.baud_rate = baud_rate
        self.sim_vsp = VirtualSerialPort(sim_port)
        self.app_vsp = VirtualSerialPort(app_port)
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
        timestamp = time.strftime('%H:%M:%S.%f')[:-3]
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

    def _is_modbus_frame(self, data):
        if len(data) < 5:
            return False
        fc = data[1]
        if 0x01 <= fc <= 0x7F:
            crc = struct.unpack('<H', data[-2:])[0]
            calculated = self._calculate_crc(data[:-2])
            return crc == calculated
        return False

    def _handle_modbus(self, data):
        if len(data) < 5:
            return None

        addr = data[0]
        fc = data[1]
        crc = struct.unpack('<H', data[-2:])[0]
        calculated_crc = self._calculate_crc(data[:-2])
        
        if crc != calculated_crc:
            print(f"CRC校验失败: 收到=0x{crc:04X}, 计算=0x{calculated_crc:04X}")
            return None

        print(f"Modbus请求: 地址=0x{addr:02X}, 功能码=0x{fc:02X}")

        channel = self.asd_channels.get(addr)
        if not channel:
            return None

        response = None

        if fc in (0x10, 0x20, 0x22):
            if len(data) >= 6:
                start_addr = struct.unpack('>H', data[2:4])[0]
                quantity = struct.unpack('>H', data[4:6])[0] if len(data) >= 6 else 0
                response = self._handle_modbus_read(channel, fc, start_addr, quantity)

        elif fc in (0x11, 0x21):
            if len(data) >= 6:
                start_addr = struct.unpack('>H', data[2:4])[0]
                byte_count = data[4] if len(data) > 4 else 0
                write_data = data[5:5+byte_count] if len(data) > 5+byte_count else data[5:-2]
                response = self._handle_modbus_write(channel, fc, start_addr, write_data)

        elif fc == 0x12:
            crc_out = self._calculate_crc(data[:-2])
            data_out = bytearray()
            data_out.extend(struct.pack('<H', crc_out))
            data_out.append(fc)
            response_crc = self._calculate_crc([addr, fc, 0x03] + list(data_out))
            response = struct.pack('>BBB', addr, fc, 0x03) + data_out + struct.pack('<H', response_crc)

        if response:
            self._log("OUT", response)
        return response

    def _handle_modbus_read(self, channel, fc, start_addr, quantity):
        channel.update_measurements()

        if fc == 0x10:
            if start_addr == 0x00 and quantity == 0x02:
                data = bytearray(24)
                data[0] = 0x01
                data[1] = 0x02
                data[2] = 0x01
                data[5] = 0x55
                data[6] = 0x01
                data[7] = 0x02
                crc = self._calculate_crc([channel.addr, fc, 0x18] + list(data))
                return struct.pack('>BBB', channel.addr, fc, 0x18) + data + struct.pack('<H', crc)

            elif start_addr == 0x02 and quantity == 0x02:
                data = bytearray()
                data.append(channel.temperature & 0xFF)
                data.extend(struct.pack('<H', 12000))
                data.append(channel.protection_flags & 0xFF)
                data.append(0x00)
                data.append(0x00)
                crc = self._calculate_crc([channel.addr, fc, 0x06] + list(data))
                return struct.pack('>BBB', channel.addr, fc, 0x06) + data + struct.pack('<H', crc)

        elif fc == 0x20:
            if start_addr == 0x10 and quantity == 0x00:
                data = bytearray()
                data.append(0x00)
                data.extend(struct.pack('<I', channel.battery_voltage_uv))
                data.extend(struct.pack('<I', channel.max_current_ma))
                data.append(channel.current_range)
                data.append(channel.display_speed)
                data.extend(struct.pack('<H', channel.internal_resistance_mohm))
                crc = self._calculate_crc([channel.addr, fc, 0x0D] + list(data))
                return struct.pack('>BBB', channel.addr, fc, 0x0D) + data + struct.pack('<H', crc)

        elif fc == 0x22:
            if start_addr == 0x10 and quantity == 0x0E:
                data = bytearray()
                data.append(1 if channel.output_enabled else 0)
                data.extend(struct.pack('<I', channel.voltage_avg))
                data.append(channel.voltage_unit)
                data.extend(struct.pack('<I', abs(channel.current_avg)))
                data.append(channel.current_unit)
                data.append(channel.current_direction)
                data.append(channel.temperature)
                data.append(channel.protection_flags)
                crc = self._calculate_crc([channel.addr, fc, 0x0E] + list(data))
                return struct.pack('>BBB', channel.addr, fc, 0x0E) + data + struct.pack('<H', crc)

        return None

    def _handle_modbus_write(self, channel, fc, start_addr, data):
        if fc == 0x11:
            if start_addr == 0x03 and len(data) >= 6:
                channel.current_range = data[0]
                channel.protection_current_ma = struct.unpack('<H', data[1:3])[0]
                channel.protection_power_w = struct.unpack('<H', data[3:5])[0]
                channel.protection_time_ms = data[5]
                print(f"通道{channel.addr} 保护设置: 量程={channel.current_range}, 保护电流={channel.protection_current_ma}mA")
                crc = self._calculate_crc([channel.addr, fc, 0x03])
                return struct.pack('>BBBH', channel.addr, fc, 0x03, crc)

            elif start_addr == 0x04 and len(data) >= 1:
                enable = data[0]
                channel.set_output(enable == 1)
                print(f"通道{channel.addr} 开关控制: {'开' if enable == 1 else '关'}")
                crc = self._calculate_crc([channel.addr, fc, 0x03])
                return struct.pack('>BBBH', channel.addr, fc, 0x03, crc)

            elif start_addr == 0x0C and len(data) >= 1:
                channel.locked = data[0] == 1
                print(f"通道{channel.addr} 面板锁屏: {'锁屏' if channel.locked else '解锁'}")
                crc = self._calculate_crc([channel.addr, fc, 0x01])
                return struct.pack('>BBBH', channel.addr, fc, 0x01, crc)

        elif fc == 0x21:
            if start_addr == 0x10 and len(data) >= 13:
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
                print(f"通道{channel.addr} 模拟电池设置: 电压={channel.battery_voltage_uv/1e6}V, 电流={channel.max_current_ma}mA")
                crc = self._calculate_crc([channel.addr, fc, 0x0D])
                return struct.pack('>BBBH', channel.addr, fc, 0x0D, crc)

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

    def _forward_data(self):
        while self.running:
            if self.sim_vsp.running and self.app_vsp.running:
                data = self.app_vsp.read(1024)
                if data:
                    self._log("IN", data)
                    response = None
                    
                    temp_buffer = self.data_buffer + data
                    if self._is_modbus_frame(temp_buffer):
                        response = self._handle_modbus(temp_buffer)
                        self.data_buffer = b""
                    elif b'\n' in temp_buffer or b'\r' in temp_buffer:
                        try:
                            text_data = temp_buffer.decode('utf-8', errors='ignore')
                            lines = text_data.replace('\r', '\n').split('\n')
                            for line in lines:
                                line = line.strip()
                                if line:
                                    cmd_response = self._handle_text_command(line)
                                    if cmd_response:
                                        if response is None:
                                            response = b""
                                        response += (cmd_response + '\r\n').encode('utf-8')
                            self.data_buffer = b""
                        except:
                            pass
                    
                    if response:
                        self._log("OUT", response)
                        self.app_vsp.write(response)
                    
                    self.sim_vsp.write(data)
            
            data = self.sim_vsp.read(1024)
            if data:
                self.app_vsp.write(data)
            
            time.sleep(0.001)

    def run(self):
        if not HAS_PYWIN32:
            print("错误: 需要安装 pywin32")
            print("请运行: pip install pywin32")
            return

        print(f"创建虚拟串口对: {self.sim_port} <-> {self.app_port}")
        print(f"波特率: {self.baud_rate}")
        
        if not self.sim_vsp.create():
            print(f"无法创建虚拟串口 {self.sim_port}")
            return
            
        if not self.app_vsp.create():
            print(f"无法创建虚拟串口 {self.app_port}")
            return

        print("等待客户端连接...")
        print(f"上位机请连接: {self.app_port}")
        print(f"模拟器监听: {self.sim_port}")

        def wait_sim():
            self.sim_vsp.wait_for_connection()
            
        def wait_app():
            self.app_vsp.wait_for_connection()
            
        t1 = threading.Thread(target=wait_sim, daemon=True)
        t2 = threading.Thread(target=wait_app, daemon=True)
        t1.start()
        t2.start()
        
        t1.join(timeout=30)
        t2.join(timeout=30)
        
        if not self.sim_vsp.running or not self.app_vsp.running:
            print("连接超时")
            return
            
        self.running = True
        print("虚拟串口对连接成功！")
        print("支持: 文本命令 | Modbus RTU")
        
        try:
            self._forward_data()
        except KeyboardInterrupt:
            print("\n收到中断信号")
            self.stop()
        except Exception as e:
            print(f"错误: {e}")

    def stop(self):
        self.running = False
        self.sim_vsp.close()
        self.app_vsp.close()
        print("虚拟串口模拟器停止")


if __name__ == '__main__':
    simulator = VirtualSerialSimulator(
        sim_port='COM2',
        app_port='COM3',
        baud_rate=9600
    )
    simulator.run()