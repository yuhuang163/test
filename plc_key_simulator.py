
#!/usr/bin/env python3
import socket
import threading
import time
import logging
from dataclasses import dataclass
from typing import Dict, Optional, Tuple
import struct

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


@dataclass
class PlcConfig:
    host: str = "0.0.0.0"
    port: int = 502
    unit_id: int = 1
    m_base: int = 200
    m_coil_offset: int = 0
    position_ready_base: int = 256
    step_done_base: int = 260
    key_done_offset: int = 10


class ModbusTcpServer:
    def __init__(self, config: PlcConfig):
        self.config = config
        self.server_socket: Optional[socket.socket] = None
        self.running = False
        self.client_socket: Optional[socket.socket] = None
        
        self.m_coils: Dict[int, bool] = {}
        self.key_cap_values: Dict[int, int] = {}
        
        self._init_default_coils()
        
    def _init_default_coils(self):
        for i in range(100):
            self.m_coils[self.config.m_base + i] = False
        self.m_coils[self.config.position_ready_base] = True
        
        for i in range(7):
            self.key_cap_values[i] = 3000
    
    def set_coil(self, address: int, value: bool):
        self.m_coils[address] = value
        logger.info(f"Set coil M{address} = {value}")
    
    def get_coil(self, address: int) -> bool:
        return self.m_coils.get(address, False)
    
    def set_key_cap_value(self, kk: int, value: int):
        self.key_cap_values[kk] = value
        logger.info(f"Set key cap KK{kk} = {value}")
    
    def get_key_cap_value(self, kk: int) -> int:
        return self.key_cap_values.get(kk, 0)
    
    def start(self):
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.config.host, self.config.port))
        self.server_socket.listen(1)
        self.running = True
        
        logger.info(f"Modbus TCP Server started on {self.config.host}:{self.config.port}")
        
        while self.running:
            try:
                self.server_socket.settimeout(1.0)
                try:
                    self.client_socket, addr = self.server_socket.accept()
                    logger.info(f"Client connected from {addr}")
                    
                    while self.running:
                        try:
                            self.client_socket.settimeout(1.0)
                            data = self.client_socket.recv(1024)
                            if not data:
                                break
                            
                            response = self._handle_modbus_request(data)
                            if response:
                                self.client_socket.sendall(response)
                                
                        except socket.timeout:
                            continue
                        except Exception as e:
                            logger.error(f"Error handling client: {e}")
                            break
                            
                except socket.timeout:
                    continue
                    
            except Exception as e:
                logger.error(f"Server error: {e}")
                if self.client_socket:
                    self.client_socket.close()
                    self.client_socket = None
    
    def _handle_modbus_request(self, data: bytes) -> Optional[bytes]:
        try:
            logger.info(f"Received raw data: {len(data)} bytes - {data.hex()}")
            
            if len(data) < 8:
                logger.warning(f"Data too short: {len(data)} bytes")
                return None
                
            transaction_id = struct.unpack('>H', data[0:2])[0]
            protocol_id = struct.unpack('>H', data[2:4])[0]
            length = struct.unpack('>H', data[4:6])[0]
            unit_id = data[6]
            function_code = data[7]
            
            if protocol_id != 0:
                logger.warning(f"Invalid protocol ID: {protocol_id}")
                return None
                
            logger.info(f"Modbus request: TID={transaction_id}, FC={function_code}, Unit={unit_id}, Length={length}")
            
            if function_code == 1:
                return self._handle_read_coils(transaction_id, unit_id, data[8:])
            elif function_code == 5:
                return self._handle_write_single_coil(transaction_id, unit_id, data[8:])
            elif function_code == 15:
                return self._handle_write_multiple_coils(transaction_id, unit_id, data[8:])
            else:
                logger.warning(f"Unsupported function code: {function_code}")
                return self._build_error_response(transaction_id, unit_id, function_code, 1)
                
        except Exception as e:
            logger.error(f"Error handling modbus request: {e}")
            return None
    
    def _handle_read_coils(self, transaction_id: int, unit_id: int, data: bytes) -> bytes:
        if len(data) < 4:
            return self._build_error_response(transaction_id, unit_id, 1, 3)
            
        start_addr = struct.unpack('>H', data[0:2])[0]
        quantity = struct.unpack('>H', data[2:4])[0]
        
        logger.debug(f"Read coils: start={start_addr}, count={quantity}")
        
        byte_count = (quantity + 7) // 8
        result = bytearray()
        
        for i in range(byte_count):
            byte_val = 0
            for bit in range(8):
                coil_addr = start_addr + i * 8 + bit
                if coil_addr < start_addr + quantity:
                    if self.m_coils.get(coil_addr, False):
                        byte_val |= (1 << bit)
            result.append(byte_val)
        
        response = struct.pack('>HHHBB', 
                               transaction_id, 0, 3 + byte_count, 
                               unit_id, byte_count) + result
        return response
    
    def _handle_write_single_coil(self, transaction_id: int, unit_id: int, data: bytes) -> bytes:
        if len(data) < 4:
            return self._build_error_response(transaction_id, unit_id, 5, 3)
            
        output_addr = struct.unpack('>H', data[0:2])[0]
        output_value = struct.unpack('>H', data[2:4])[0]
        
        coil_state = output_value == 0xFF00
        self.set_coil(output_addr, coil_state)
        
        response = struct.pack('>HHHBBHH', 
                               transaction_id, 0, 7, 
                               unit_id, function_code, output_addr, output_value)
        return response
    
    def _handle_write_multiple_coils(self, transaction_id: int, unit_id: int, data: bytes) -> bytes:
        if len(data) < 5:
            return self._build_error_response(transaction_id, unit_id, 15, 3)
            
        start_addr = struct.unpack('>H', data[0:2])[0]
        quantity = struct.unpack('>H', data[2:4])[0]
        byte_count = data[4]
        
        if len(data) < 5 + byte_count:
            return self._build_error_response(transaction_id, unit_id, 15, 3)
        
        coil_data = data[5:5+byte_count]
        
        for i in range(quantity):
            byte_idx = i // 8
            bit_idx = i % 8
            coil_addr = start_addr + i
            
            if byte_idx < len(coil_data):
                coil_state = bool(coil_data[byte_idx] & (1 << bit_idx))
                self.set_coil(coil_addr, coil_state)
        
        response = struct.pack('>HHH BHH', 
                               transaction_id, 0, 6, 
                               unit_id, start_addr, quantity)
        return response
    
    def _build_error_response(self, transaction_id: int, unit_id: int, 
                              function_code: int, exception_code: int) -> bytes:
        error_function_code = function_code | 0x80
        return struct.pack('>HHHBB', 
                          transaction_id, 0, 3, 
                          unit_id, error_function_code, exception_code)
    
    def stop(self):
        self.running = False
        if self.client_socket:
            self.client_socket.close()
        if self.server_socket:
            self.server_socket.close()
        logger.info("Modbus TCP Server stopped")


class PlcKeyTestSimulator:
    def __init__(self, config: PlcConfig):
        self.config = config
        self.modbus_server = ModbusTcpServer(config)
        self.simulation_thread: Optional[threading.Thread] = None
        self.key_test_active = False
        self.current_key_index = -1
        
    def start_simulation(self):
        self.simulation_thread = threading.Thread(target=self.modbus_server.start, daemon=True)
        self.simulation_thread.start()
        logger.info("PLC Key Test Simulation started")
    
    def stop_simulation(self):
        self.modbus_server.stop()
        if self.simulation_thread:
            self.simulation_thread.join(timeout=2)
        logger.info("PLC Key Test Simulation stopped")
    
    def simulate_key_press(self, key_index: int, delay_ms: int = 100):
        self.current_key_index = key_index
        self.key_test_active = True
        
        key_m = self.config.m_base + key_index
        key_done_m = self.config.m_base + self.config.key_done_offset
        
        logger.info(f"Simulating key press: KeyIndex={key_index}, KeyM=M{key_m}")
        
        def press_simulation():
            time.sleep(delay_ms / 1000.0)
            
            self.modbus_server.set_coil(key_done_m, True)
            logger.info(f"Key press completed: KeyDoneM=M{key_done_m} = True")
            
            time.sleep(0.1)
            self.modbus_server.set_coil(key_done_m, False)
            
            self.key_test_active = False
        
        threading.Thread(target=press_simulation, daemon=True).start()
    
    def simulate_key_test_sequence(self, key_indices: list, key_caps: dict = None):
        if key_caps is None:
            key_caps = {}
            
        for kk, cap_value in key_caps.items():
            self.modbus_server.set_key_cap_value(kk, cap_value)
        
        for key_index in key_indices:
            if not self.modbus_server.running:
                break
                
            self.simulate_key_press(key_index)
            time.sleep(0.5)
    
    def get_status(self) -> dict:
        return {
            'running': self.modbus_server.running,
            'key_test_active': self.key_test_active,
            'current_key_index': self.current_key_index,
            'm_coils': dict(self.modbus_server.m_coils),
            'key_cap_values': dict(self.modbus_server.key_cap_values)
        }


def main():
    config = PlcConfig(
        host="0.0.0.0",
        port=502,
        unit_id=1,
        m_base=200,
        m_coil_offset=0,
        position_ready_base=256,
        step_done_base=260,
        key_done_offset=10
    )
    
    simulator = PlcKeyTestSimulator(config)
    
    try:
        simulator.start_simulation()
        
        print("PLC Key Test Simulator is running...")
        print("Commands:")
        print("  status - Show current status")
        print("  press <index> - Simulate key press (0-6)")
        print("  setcap <kk> <value> - Set key cap value")
        print("  sequence - Run full key test sequence")
        print("  quit - Exit simulator")
        
        while True:
            try:
                cmd = input("> ").strip().lower()
                
                if cmd == 'quit':
                    break
                elif cmd == 'status':
                    status = simulator.get_status()
                    print(f"Status: {status}")
                elif cmd.startswith('press '):
                    parts = cmd.split()
                    if len(parts) == 2:
                        key_index = int(parts[1])
                        simulator.simulate_key_press(key_index)
                elif cmd.startswith('setcap '):
                    parts = cmd.split()
                    if len(parts) == 3:
                        kk = int(parts[1])
                        value = int(parts[2])
                        simulator.modbus_server.set_key_cap_value(kk, value)
                        print(f"Set KK{kk} = {value}")
                elif cmd == 'sequence':
                    print("Running key test sequence...")
                    simulator.simulate_key_test_sequence(
                        key_indices=[0, 1, 2, 3, 4, 5, 6],
                        key_caps={0: 3500, 1: 3200, 2: 3800, 3: 3400, 4: 3600, 5: 3300, 6: 3700}
                    )
                    print("Sequence completed")
                else:
                    print("Unknown command")
                    
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Error: {e}")
                
    finally:
        simulator.stop_simulation()


if __name__ == "__main__":
    main()