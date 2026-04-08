from hm10_esp32 import HM10ESP32BridgeOrigin
import time
import sys
import threading
import datetime

PORT = 'COM8'
EXPECTED_NAME = 'BT4'

# Globals for move sequence
moveSq = [1, 3, 2, 3, 4, 3] * 167
move_index = 0
waiting_for_ack = False
current_pending_move = None

def background_listener(bridge):
    global move_index, waiting_for_ack, current_pending_move
    while True:
        messages = bridge.listen()
        for msg in messages:
            msg = msg.strip()
            if not msg: continue
                
            timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Handle Next Move Receipt Confirmation with value verification
            if "nxtMoveRecived:" in msg:
                try:
                    # Parse the move value sent back by Arduino
                    parts = msg.split(":")
                    if len(parts) >= 2:
                        received_move = int(parts[1].strip())
                        
                        if waiting_for_ack and received_move == current_pending_move:
                            print(f"[{timestamp}] ✅ Confirmation received for move: {current_pending_move}")
                            waiting_for_ack = False
                            current_pending_move = None
                        elif waiting_for_ack:
                            print(f"[{timestamp}] ❌ Mismatch! Arduino has {received_move}, expected {current_pending_move}. Resending...")
                        else:
                            print(f"[{timestamp}] ℹ️ Arduino currently has move: {received_move} (ready for next)")
                except (ValueError, IndexError):
                    print(f"[{timestamp}] ⚠️ Malformed ACK received: {msg}")
                continue

            # Handle Next Move Request
            if "reqNxtMove" in msg:
                if waiting_for_ack:
                    # Arduino is re-asking because it didn't get the last one or ACK was lost
                    response = f"nxtMove:{current_pending_move}"
                    bridge.send(response)
                    print(f"[{timestamp}] 🤖 Request: reqNxtMove -> Re-sending Pending: {response}")
                else:
                    if move_index < len(moveSq):
                        next_move = moveSq[move_index]
                        move_index += 1
                        current_pending_move = next_move
                        waiting_for_ack = True
                        response = f"nxtMove:{next_move}"
                        bridge.send(response)
                        print(f"[{timestamp}] 🤖 Request: reqNxtMove -> Sent New: {response}")
                    else:
                        print(f"[{timestamp}] 🤖 Request: reqNxtMove -> No more moves!")
                continue

            # Handle RFID messages
            if msg.startswith("RFID:"):
                uid = msg[5:].strip()
                response = f"rfidAck:{uid}"
                bridge.send(response)
                print(f"[{timestamp}] 🏷️ RFID: {uid} -> ACK Sent: {response}")
                continue

            # Handle other messages
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
