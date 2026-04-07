from hm10_esp32 import HM10ESP32BridgeOrigin
import time
import sys
import threading
import datetime

PORT = 'COM12'
EXPECTED_NAME = 'BT4'

# Globals for move sequence
moveSq = [1, 3, 2, 3, 4, 3] * 167
move_index = 0

def background_listener(bridge):
    global move_index
    while True:
        messages = bridge.listen()
        for msg in messages:
            msg = msg.strip()
            if not msg: continue
                
            timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Handle Next Move Request
            if "reqNxtMove" in msg:
                if move_index < len(moveSq):
                    next_move = moveSq[move_index]
                    move_index += 1
                    response = f"nxtMove:{next_move}"
                    bridge.send(response)
                    print(f"[{timestamp}] 🤖 Request: reqNxtMove -> Sent: {response}")
                else:
                    print(f"[{timestamp}] 🤖 Request: reqNxtMove -> No more moves!")
                continue

            # Handle RFID or other messages
            print(f"[{timestamp}] HM10: {msg}")

        time.sleep(0.05)

def main():
    bridge = HM10ESP32BridgeOrigin(port=PORT)
    
    # 1. Configuration Check
    print("Checking ESP32 configuration...")
    current_name = bridge.get_hm10_name()
    if current_name != EXPECTED_NAME:
        print(f"Target mismatch. Current: '{current_name}', Expected: '{EXPECTED_NAME}'")
        print(f"Updating target name to {EXPECTED_NAME}...")
        if bridge.set_hm10_name(EXPECTED_NAME):
            print("✅ Name updated successfully. Resetting ESP32...")
            bridge.reset()
        else:
            print("❌ Failed to set name. Exiting.")
            sys.exit(1)

    # 2. Connection Check
    print("Verifying connection...")
    status = bridge.get_status()
    if status != "CONNECTED":
        print(f"ESP32 is {status}, retrying...")
        time.sleep(3)
        status = bridge.get_status()
        
    if status != "CONNECTED":
        print(f"⚠️ ESP32 is {status}. Please ensure HM-10 is advertising. Exiting.")
        sys.exit(0)

    print(f"✨ Ready! Connected to {EXPECTED_NAME}")
    
    # Start background listener thread
    listener_thread = threading.Thread(target=background_listener, args=(bridge,), daemon=True)
    listener_thread.start()

    # Main loop - Command Interface
    print("Remote Monitor is active. Type commands (e.g., 'STOP', 'SPEED_100').")
    print("Type 'exit' to stop.")
    try:
        while True:
            user_msg = input()
            if user_msg.lower() in ['exit', 'quit']: break
            if user_msg: bridge.send(user_msg)
    except KeyboardInterrupt:
        pass
            
    print("\nApplication closed.")

if __name__ == "__main__":
    main()
