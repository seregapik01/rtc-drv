from smbus2 import SMBus
import time

class RTCEmulator:
    def __init__(self, bus_number, address):
        self.bus = SMBus(bus_number)
        self.address = address
        self.registers = [0] * 256  # Эмулируем 256 регистров

    def read_byte_data(self, reg):
        return self.registers[reg]

    def write_byte_data(self, reg, value):
        self.registers[reg] = value

    def run(self):
        print("HT74563A RTC emulator started")
        while True:
            time.sleep(1)

if __name__ == "__main__":
    rtc = RTCEmulator(bus_number=1, address=0x68)
    rtc.run()