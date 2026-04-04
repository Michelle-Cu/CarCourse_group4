from hm10_esp32 import HM10ESP32Bridge
import time
import sys
import threading
import csv
import datetime
import os

PORT = 'COM8'
EXPECTED_NAME = 'HM10_4'

# Globals for average calculations
avgLoopTime = 0.0
avgIrCycleTime = 0.0
loopSamples = 0
irSamples = 0

# Globals for move sequence
moveSq = [1, 3, 2, 3, 4, 3] * 167
move_index = 0

def background_listener(bridge, csv_writer, csv_file):
    global avgLoopTime, avgIrCycleTime, loopSamples, irSamples, move_index
    while True:
        messages = bridge.listen()
        for msg in messages:
            msg = msg.strip()
            if not msg:
                continue
                
            timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Handle Next Move Request
            if msg == "reqNxtMove":
                if move_index < len(moveSq):
                    next_move = moveSq[move_index]
                    move_index += 1
                    response = f"nxtMove:{next_move}"
                    bridge.send(response)
                    print(f"[{timestamp}] 🤖 Request: reqNxtMove -> Sent: {response}")
                else:
                    print(f"[{timestamp}] 🤖 Request: reqNxtMove -> No more moves!")
                continue

            # Handle RFID messages
            if msg.startswith("RFID:"):
                print(f"[{timestamp}] 🔑 {msg}")
                continue

            # Try to log as data if it matches the 9-part format
            parts = msg.split(',')
            if len(parts) == 9:
                try:
                    loopCnt = int(parts[0])
                    thisLoopTime = int(parts[1])
                    irCnt = int(parts[2])
                    thisIrTime = int(parts[3])
                    
                    # Calculate averages based on session samples
                    loopSamples += 1
                    avgLoopTime = (avgLoopTime * (loopSamples - 1) + thisLoopTime) / loopSamples
                    
                    irSamples += 1
                    avgIrCycleTime = (avgIrCycleTime * (irSamples - 1) + thisIrTime) / irSamples
                    
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
                except (ValueError, IndexError):
                    # If it's not data, print it anyway
                    print(f"[{timestamp}] HM10: {msg}")
            else:
                # Print other messages for monitoring (e.g. status updates)
                print(f"[{timestamp}] HM10: {msg}")

        time.sleep(0.05)

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
            # bridge = HM10ESP32Bridge(port=PORT)
        else:
            print("❌ Failed to set name. Exiting.")
            sys.exit(1)

    # 2. Connection Check
    print("Verifying connection...")
    time.sleep(2) # Give it a moment to pair
    status = bridge.get_status()
    if status != "CONNECTED":
        # One retry
        print(f"ESP32 is {status}, retrying...")
        time.sleep(3)
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
    
    # Flush buffers to ensure we start in sync
    bridge.flush()
    
    with open(filename, mode='w', newline='') as f:
        csv_writer = csv.writer(f)
        csv_writer.writerow([
            'Timestamp', 'LoopCnt', 'LoopTime(ms)', 'AvgLoopTime(ms)',
            'IrCnt', 'IrTime(ms)', 'AvgIrTime(ms)',
            'IR1', 'IR2', 'IR3', 'IR4', 'IR5'
        ])
        print(f"Logging data to {filename}...")
        
        # Start background listener thread
        listener_thread = threading.Thread(
            target=background_listener, 
            args=(bridge, csv_writer, f), 
            daemon=True
        )
        listener_thread.start()

        # Main loop - Remote Monitor Command Interface
        print("Data logger is running. Type commands to send to the robot (e.g., 'STOP', 'SPEED_100').")
        print("Type 'exit' or press Ctrl+C to stop.")
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
