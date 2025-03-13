from time import sleep
import serial, json, reedsolo

# Initialize the Reed-Solomon encoder
rs = reedsolo.RSCodec(32)  # 32 parity symbols
ser = serial.Serial("/dev/ttyUSB0", 9600, timeout=1)

while True:
    if ser.in_waiting > 0:
        raw_data = ser.readline().decode('utf-8', errors='ignore')
        try:
            print(f"\033[92mValid JSON data received: {json.loads(raw_data)}\033[0m")
            
        except json.JSONDecodeError:
            print(f"\033[91mInvalid JSON data received: {raw_data}\033[0m")
    else:
        print("\033[93mNo data received\033[0m")
        sleep(0.2)
