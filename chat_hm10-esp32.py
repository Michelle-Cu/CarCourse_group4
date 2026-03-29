from hm10_esp32 import HM10ESP32Bridge
import time
import sys
import threading
import csv
import datetime
import os

PORT = 'COM9'
EXPECTED_NAME = 'HM10-10'

# Globals for average calculations
avgLoopTime = 0.0
avgIrCycleTime = 0.0

def background_listener(bridge, csv_writer, csv_file):
    global avgLoopTime, avgIrCycleTime
    while True:
        messages = bridge.listen()
        for msg in messages:
            msg = msg.strip()
            if not msg:
                continue
                
            timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Print received message
            print(f"[{timestamp}] HM10: {msg}")
            
            # Data format from Arduino: 
            # loopCycleCnt,thisLoopTime,irCycleCnt,thisIrTime,IR1..5,Thresh1..5 (14 values)
            parts = msg.split(',')
            if len(parts) == 14:
                try:
                    loopCnt = int(parts[0])
                    thisLoopTime = int(parts[1])
                    irCnt = int(parts[2])
                    thisIrTime = int(parts[3])
                    
                    # Calculate averages in Python
                    if loopCnt > 0:
                        avgLoopTime = (avgLoopTime * (loopCnt - 1) + thisLoopTime) / loopCnt
                    if irCnt > 0:
                        avgIrCycleTime = (avgIrCycleTime * (irCnt - 1) + thisIrTime) / irCnt
                    
                    # Construct row for CSV:
                    # Timestamp, LoopCnt, LoopTime, AvgLoopTime, IrCnt, IrTime, AvgIrTime, IR1..5, T1..5
                    row = [
                        timestamp, 
                        loopCnt, 
                        thisLoopTime, 
                        f"{avgLoopTime:.2f}",
                        irCnt, 
                        thisIrTime, 
                        f"{avgIrCycleTime:.2f}"
                    ] + parts[4:]
                    
                    csv_writer.writerow(row)
                    csv_file.flush()
                except (ValueError, IndexError) as e:
                    print(f"Error parsing data: {e}")
            elif len(parts) > 0:
                # If it's not the main data packet, still log it if possible or just print
                pass

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
    data_dir = "data"
    if not os.path.exists(data_dir):
        os.makedirs(data_dir)
        
    filename = os.path.join(data_dir, f"robot_data_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    
    with open(filename, mode='w', newline='') as f:
        csv_writer = csv.writer(f)
        # Write header matching the NEW data format
        csv_writer.writerow([
            'Timestamp', 'LoopCnt', 'LoopTime(ms)', 'AvgLoopTime(ms)',
            'IrCnt', 'IrTime(ms)', 'AvgIrTime(ms)',
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
