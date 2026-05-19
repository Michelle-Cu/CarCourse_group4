import socket
import time
import random

# =================================================================
# CONFIGURATION
# =================================================================
# Replace with your ESP32's Bluetooth MAC address.
# On Linux/RPi, run `hcitool scan` or `bluetoothctl devices` to find it.
ESP32_MAC_ADDRESS = "XX:XX:XX:XX:XX:XX" 
PORT = 1 # RFCOMM port for SPP is usually 1

def read_sensors():
    """
    MOCK SENSOR DATA.
    Replace this with your actual sensor reading logic (MPU6050, ADC for Flex, etc.)
    """
    # 6-Axis (Acc/Gyro)
    acc = [random.uniform(-1, 1) for _ in range(3)]
    gyro = [random.uniform(-10, 10) for _ in range(3)]
    
    # Flex sensors (usually 0-1023 or 0-4095 depending on ADC)
    flex = [random.randint(0, 1023) for _ in range(2)]
    
    return acc + gyro + flex

def main():
    print(f"Connecting to ESP32 ({ESP32_MAC_ADDRESS})...")
    
    try:
        # Create a Bluetooth RFCOMM socket
        sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
        sock.connect((ESP32_MAC_ADDRESS, PORT))
        print("Connected successfully!")
        
        while True:
            # 1. Read sensors
            sensor_data = read_sensors()
            
            # 2. Format string: S:<acc_x>,<acc_y>,<acc_z>,<gyro_x>,<gyro_y>,<gyro_z>,<flex_1>,<flex_2>
            # Matching the format in arm_controller.ino
            data_str = "S:" + ",".join([f"{val:.2f}" if isinstance(val, float) else str(val) for val in sensor_data]) + "\n"
            
            # 3. Send
            sock.send(data_str.encode('utf-8'))
            print(f"Sent: {data_str.strip()}")
            
            # 4. Wait
            time.sleep(0.05) # 20Hz update rate
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'sock' in locals():
            sock.close()
            print("Socket closed.")

if __name__ == "__main__":
    main()
