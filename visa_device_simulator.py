import socket
import threading
import time
import random

class VisaDeviceSimulator:
    def __init__(self, host='localhost', port=5025):
        self.host = host
        self.port = port
        self.socket = None
        self.running = False
        self.voltage = 12.0
        self.current = 2.5
        self.output_enabled = False
        self.measured_voltage = 0.0
        self.measured_current = 0.0
        self.lock = threading.Lock()

    def handle_command(self, command):
        command = command.strip()
        
        if not command:
            return ""
        
        print(f"Received command: {command}")
        
        if command.startswith('AT+'):
            return self.handle_at_command(command)
        
        with self.lock:
            scpi_match = self._parse_scpi(command)
            if scpi_match:
                cmd, params, is_query = scpi_match
                return self._handle_scpi(cmd, params, is_query)
            
            if command.startswith('VOLT'):
                try:
                    parts = command.split()
                    if len(parts) >= 2:
                        self.voltage = float(parts[1])
                        print(f"Set voltage to {self.voltage}V")
                    return "OK"
                except ValueError:
                    return "ERROR"
            
            elif command.startswith('CURR'):
                try:
                    parts = command.split()
                    if len(parts) >= 2:
                        self.current = float(parts[1])
                        print(f"Set current to {self.current}A")
                    return "OK"
                except ValueError:
                    return "ERROR"
            
            elif command.startswith('OUTP'):
                parts = command.split()
                if len(parts) >= 2:
                    if parts[1].upper() == 'ON':
                        self.output_enabled = True
                        print("Output turned ON")
                        # 模拟输出开启后的测量值
                        self.measured_voltage = self.voltage * (0.99 + random.random() * 0.02)
                        self.measured_current = 0.01 + random.random() * 0.02  # 模拟小电流
                    elif parts[1].upper() == 'OFF':
                        self.output_enabled = False
                        print("Output turned OFF")
                        self.measured_voltage = 0.0
                        self.measured_current = 0.0
                return "OK"
            
            elif command.startswith('MEAS:VOLT:DC?'):
                if self.output_enabled:
                    # 添加一些噪声模拟真实测量
                    noise = (random.random() - 0.5) * 0.1
                    value = self.measured_voltage + noise
                    return f"{value:.4f}"
                else:
                    return "0.0"
            
            elif command.startswith('MEAS:CURR:DC?'):
                if self.output_enabled:
                    # 添加一些噪声模拟真实测量
                    noise = (random.random() - 0.5) * 0.001
                    value = self.measured_current + noise
                    return f"{value:.6f}"
                else:
                    return "0.0"
            
            elif command.startswith('*IDN?'):
                return "VISA_DEVICE_SIMULATOR,MODEL_001,VERSION_1.0"
            
            elif command.startswith('SYST:ERR?'):
                return "0,No error"
            
            elif command == 'RST':
                self.voltage = 12.0
                self.current = 2.5
                self.output_enabled = False
                self.measured_voltage = 0.0
                self.measured_current = 0.0
                print("Device reset")
                return "OK"
            
            else:
                print(f"Unknown command: {command}")
                return "ERROR: Unknown command"

    def _parse_scpi(self, command):
        import re
        match = re.match(r'^([A-Za-z]+)(\d*):([A-Za-z]+)(\d*):([A-Za-z]+)(\d*):([A-Za-z]+)(\d*):([A-Za-z]+)(\d*)(?:\s+(.+))?(?:\?)?$', command)
        if match:
            parts = match.groups()
            cmd_path = []
            for i in range(0, len(parts)-1, 2):
                if parts[i]:
                    cmd_path.append(parts[i].upper())
                if parts[i+1]:
                    cmd_path.append(parts[i+1])
            param = parts[-1] if parts[-1] else None
            is_query = command.endswith('?')
            return ('_'.join(cmd_path), param, is_query)
        return None

    def _handle_scpi(self, cmd, param, is_query):
        if cmd == 'SOURCE1_VOLTAGE_LEVEL_IMMEDIATE_AMPLITUDE':
            if is_query:
                return f"{self.voltage:.3f}"
            else:
                try:
                    self.voltage = float(param)
                    print(f"SCPI Set voltage to {self.voltage}V")
                    return ""
                except ValueError:
                    return "ERROR"
        elif cmd.startswith('SOURCE') and cmd.endswith('VOLTAGE'):
            if is_query:
                return f"{self.voltage:.3f}"
            else:
                try:
                    self.voltage = float(param)
                    print(f"SCPI Set voltage to {self.voltage}V")
                    return ""
                except ValueError:
                    return "ERROR"
        elif cmd.startswith('SOURCE') and cmd.endswith('CURRENT'):
            if is_query:
                return f"{self.current:.6f}"
            else:
                try:
                    self.current = float(param)
                    print(f"SCPI Set current to {self.current}A")
                    return ""
                except ValueError:
                    return "ERROR"
        elif cmd.startswith('OUTPUT'):
            if is_query:
                return "1" if self.output_enabled else "0"
            else:
                try:
                    value = int(param)
                    self.output_enabled = (value != 0)
                    print(f"SCPI Set output {'ON' if self.output_enabled else 'OFF'}")
                    if self.output_enabled:
                        self.measured_voltage = self.voltage * (0.99 + random.random() * 0.02)
                        self.measured_current = 0.01 + random.random() * 0.02
                    else:
                        self.measured_voltage = 0.0
                        self.measured_current = 0.0
                    return ""
                except ValueError:
                    return "ERROR"
        return None

    def handle_at_command(self, command):
        if command.startswith('AT+DCON='):
            mac_addr = command[8:]
            print(f"Bluetooth direct connect: {mac_addr}")
            return "OK"
        
        elif command.startswith('AT+GMAC'):
            print("Get MAC address")
            return "DF:A1:10:00:17:27"
        
        elif command.startswith('AT+GMI'):
            print("Get manufacturer")
            return "BYD"
        
        elif command.startswith('AT+GMM'):
            print("Get model")
            return "V3Pro"
        
        elif command.startswith('AT+GMR'):
            print("Get revision")
            return "1.0.0"
        
        elif command.startswith('AT+BLEPWR'):
            parts = command.split('=')
            if len(parts) > 1:
                print(f"Set BLE power: {parts[1]}")
            return "OK"
        
        elif command == 'AT':
            return "OK"
        
        else:
            print(f"Unknown AT command: {command}")
            return "ERROR"

    def handle_client(self, conn, addr):
        print(f"Connected by {addr}")
        buffer = ""
        try:
            while self.running:
                data = conn.recv(1024)
                if not data:
                    break
                
                buffer += data.decode('utf-8')
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    response = self.handle_command(line)
                    if response:
                        conn.sendall((response + '\n').encode('utf-8'))
        except Exception as e:
            print(f"Client error: {e}")
        finally:
            conn.close()
            print(f"Disconnected from {addr}")

    def start(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((self.host, self.port))
        self.socket.listen(1)
        self.socket.settimeout(1.0)
        self.running = True
        
        print(f"VISA Device Simulator started on {self.host}:{self.port}")
        print("Simulating programmable power supply...")
        print("Supported commands: VOLT, CURR, OUTP ON/OFF, MEAS:VOLT:DC?, MEAS:CURR:DC?, *IDN?, SYST:ERR?, RST")
        
        while self.running:
            try:
                conn, addr = self.socket.accept()
                client_thread = threading.Thread(target=self.handle_client, args=(conn, addr))
                client_thread.daemon = True
                client_thread.start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"Server error: {e}")

    def stop(self):
        self.running = False
        if self.socket:
            self.socket.close()
        print("VISA Device Simulator stopped")

def main():
    simulator = VisaDeviceSimulator(host='0.0.0.0', port=5026)
    
    try:
        simulator.start()
    except KeyboardInterrupt:
        print("\nReceived shutdown signal")
        simulator.stop()

if __name__ == '__main__':
    main()