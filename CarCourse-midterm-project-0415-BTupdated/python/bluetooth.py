import sys
import time
import logging
import os

# Add the root directory and the project directory to sys.path
# to ensure hm10_esp32 can be found regardless of where the script is run from.
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir) # CarCourse-midterm-project-0415-BTupdated
workspace_root = os.path.dirname(project_root) # C:\Codes\CarCourse_group4

for path in [project_root, workspace_root]:
    if path not in sys.path:
        sys.path.insert(0, path)

try:
    # Try importing from the project-specific folder first
    from hm10_esp32.hm10_esp32_bridge_original import HM10ESP32Bridge as HM10ESP32BridgeOrigin
except ImportError:
    # Fallback to a simpler import if the package structure is different
    try:
        from hm10_esp32 import HM10ESP32BridgeOrigin
    except ImportError as e:
        logging.error(f"Could not import HM10ESP32BridgeOrigin: {e}")
        raise

def setup_bluetooth(port, expected_name, log):
    """
    Sets up the HM10-ESP32 bridge, verifies the name, and checks connection status.
    Referenced from hm10-esp32.py logic.
    """
    log.info(f"Connecting to ESP32 on {port}...")
    try:
        bridge = HM10ESP32BridgeOrigin(port=port)
    except Exception as e:
        log.error(f"Failed to open serial port {port}: {e}")
        return None

    # 1. Configuration Check
    log.info("Checking ESP32 configuration...")
    current_name = bridge.get_hm10_name()
    if current_name is None:
        log.warning("Could not retrieve current name (timeout).")
    elif current_name != expected_name:
        log.info(f"Target mismatch. Current: '{current_name}', Expected: '{expected_name}'")
        log.info(f"Updating target name to {expected_name}...")
        if bridge.set_hm10_name(expected_name):
            log.info("✅ Name updated successfully. Resetting ESP32...")
            if bridge.reset():
                log.info("Reset successful. ESP32 is rebooting.")
                # After reset, we might need a longer wait or re-init, 
                # but bridge.reset() already waits 6 seconds.
            else:
                log.error("❌ Reset failed.")
                return None
        else:
            log.error("❌ Failed to set name.")
            return None

    # 2. Connection Check
    log.info("Verifying connection status...")
    status = bridge.get_status()
    if status != "CONNECTED":
        log.info(f"ESP32 is {status}, retrying in 3 seconds...")
        time.sleep(3)
        status = bridge.get_status()
        
    if status != "CONNECTED":
        log.warning(f"⚠️ ESP32 is {status}. Please ensure HM-10 is advertising.")
    else:
        log.info(f"✨ Ready! Connected to {expected_name}")
    
    return bridge
