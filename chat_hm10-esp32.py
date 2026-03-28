from hm10_esp32 import HM10ESP32Bridge
import time
import sys
import threading
import csv
import datetime

PORT = 'COM9'
EXPECTED_NAME = 'HM10-10'

def background_listener(bridge, csv_writer, csv_file):
    while True:
        msg = bridge.listen()
        if msg:
            msg = msg.strip()
            timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Print received message
            print(f"[{timestamp}] HM10: {msg}")
            
            # Try to write to CSV if it looks like our expected data format
            parts = msg.split(',')
            if len(parts) >= 16:  # LoopCnt,LoopTime,AvgLoopTime,IrCnt,IrTime,AvgIrTime,IR1..5,Thresh1..5
                try:
                    csv_writer.writerow([timestamp] + parts)
                    csv_file.flush()
                except Exception as e:
                    print(f"Error writing to CSV: {e}")
        time.sleep(0.01) # Poll slightly faster to catch all cycles

def main():
    bridge = HM10ESP32Bridge(port=PORT)
    
    # 1. Configuration Check
    current_name = bridge.get_hm10_name()
    if current_name != EXPECTED_NAME:
        print(f"Target mismatch. Current: {current_name}, Expected: {EXPECTED_NAME}")
        print(f"Updating target name to {EXPECTED_NAME}...")
        
        if bridge.set_hm10_name(EXPECTED_NAME):
            print("✅ Name updated successfully. Resetting ESP32...")
            bridge.reset()
        else:
            print("❌ Failed to set name. Exiting.")
            sys.exit(1)

    # 2. Connection Check
    status = bridge.get_status()
    if status != "CONNECTED":
        print(f"⚠️ ESP32 is {status}. Please ensure HM-10 is advertising. Exiting.")
        sys.exit(0)

    print(f"✨ Ready! Connected to {EXPECTED_NAME}")
    
    # 3. Setup CSV Logging
    filename = f"robot_data_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    
    with open(filename, mode='w', newline='') as f:
        csv_writer = csv.writer(f)
        # Write header matching the Arduino data format
        csv_writer.writerow([
            'Timestamp', 'LoopCnt', 'LoopTime(ms)', 
            'IrCnt', 'IrTime(ms)', 
            'IR1', 'IR2', 'IR3', 'IR4', 'IR5', 
            'T1', 'T2', 'T3', 'T4', 'T5'
        ])
        print(f"Logging data to {filename}...")
        
        # Start background listener thread
        listener_thread = threading.Thread(
            target=background_listener, 
            args=(bridge, csv_writer, f), 
            daemon=True
        )
        listener_thread.start()

        # Main loop - ready for future commands to be sent to ESP32
        print("Type 'exit' to quit. You can also type commands to send to the Arduino (e.g., 'TURN_LEFT').")
        try:
            while True:
                user_msg = input()
                if user_msg.lower() in ['exit', 'quit']: 
                    break
                if user_msg: 
                    bridge.send(user_msg)
        except KeyboardInterrupt:
            pass
            
    print("\nData logging stopped. Application closed.")

if __name__ == "__main__":
    main()
