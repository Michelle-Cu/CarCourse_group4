import serial
import time
import re

class HM10ESP32Bridge:
    def __init__(self, port, rx_timeout=0.1):
        self.ser = serial.Serial(port=port, baudrate=115200, timeout=rx_timeout)
        # Matches 'bt_com' tag logs from ESP32
        self.log_regex = re.compile(r'bt_com:\s*(.*)')
        # Strips ANSI color codes often sent by ESP-IDF
        self.ansi_regex = re.compile(r'\x1b\[[0-9;]*m')
        self.msg_accumulator = ""
        self.raw_accumulator = ""
        self.in_sync = False 
        time.sleep(1) 

    def flush(self):
        """Clears serial buffers and internal accumulators to sync with fresh data."""
        self.ser.read_all()
        self.msg_accumulator = ""
        self.raw_accumulator = ""
        self.in_sync = False

    def _read_bt_com_payloads(self):
        """Reads and cleans all 'bt_com' tagged logs currently in buffer, joining fragments."""
        if self.ser.in_waiting > 0:
            try:
                self.raw_accumulator += self.ser.read_all().decode('utf-8', errors='ignore')
            except Exception:
                pass
            
        if not self.raw_accumulator:
            return []

        complete_messages = []
        # Process complete lines from the raw serial stream
        while '\n' in self.raw_accumulator or '\r' in self.raw_accumulator:
            m = re.search(r'[\r\n]', self.raw_accumulator)
            if not m: break
            
            line = self.raw_accumulator[:m.start()]
            # Advance accumulator past the newline/cr
            self.raw_accumulator = self.raw_accumulator[m.end():]
            # Handle \r\n as a single break
            if m.group() == '\r' and self.raw_accumulator.startswith('\n'):
                self.raw_accumulator = self.raw_accumulator[1:]
            
            if not line.strip(): continue
            
            match = self.log_regex.search(line)
            if match:
                # Clean ANSI colors and whitespace
                payload = self.ansi_regex.sub('', match.group(1)).strip()
                if not payload: continue

                # AT COMMAND RESPONSES: Always bypass sync logic
                if payload.startswith("OK+") or payload.startswith("ERROR+"):
                    complete_messages.append(payload)
                    continue

                # DATA STREAM SYNC: If not in sync, we wait for a fragment that looks like 
                # the start of a packet OR we wait for a newline to reset.
                if not self.in_sync:
                    # If this payload contains a comma, it's likely data.
                    # We only start syncing AFTER we've seen a newline from a previous (partial) packet.
                    # Since we can't see the newline (it was split), we use the next log line as a proxy.
                    # Simplified: if the previous packet was "broken", this new log line is a fresh start.
                    self.in_sync = True 
                    self.msg_accumulator = ""

                self.msg_accumulator += payload
                
                # Check if we have a full packet (13 commas = 14 parts)
                if self.msg_accumulator.count(',') >= 13:
                    # If there's more than 13 commas, we might have multiple packets
                    parts = self.msg_accumulator.split(',')
                    while len(parts) >= 14:
                        msg_parts = parts[:14]
                        complete_messages.append(",".join(msg_parts).strip())
                        parts = parts[14:]
                    self.msg_accumulator = ",".join(parts)
                
                # If the payload had a trailing character that suggests a break in the Arduino's transmission
                # (The ESP32 firmware usually sends a new log line when the Arduino sends \n)
                # For safety, if the accumulator gets too long, we flush it.
                if len(self.msg_accumulator) > 500:
                    self.msg_accumulator = ""
                    self.in_sync = False

        return complete_messages

    def set_hm10_name(self, name, timeout=3.0):
        """
        Sends AT+NAME<name> and verifies OK+SET<name> reply.
        Returns True on success, False on timeout/failure.
        """
        self.ser.flushInput()
        command = f"AT+NAME{name}\r\n"
        self.ser.write(command.encode('utf-8'))
        
        # Poll for the specific OK+SET response
        pattern = re.compile(fr"OK\+SET[:\s]*{re.escape(name)}")
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            for entry in self._read_bt_com_payloads():
                if pattern.search(entry):
                    return True
            time.sleep(0.05)
        return False

    def get_hm10_name(self, timeout=2.0):
        """Queries the device name currently in NVS."""
        self.ser.flushInput()
        self.ser.write(b"AT+NAME?\r\n")
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            for entry in self._read_bt_com_payloads():
                if "OK+NAME" in entry:
                    return entry.replace("OK+NAME", "").replace(":", "").strip()
            time.sleep(0.05)
        return None

    def get_status(self, timeout=2.0):
        """Checks connection status via AT+STATUS?."""
        self.ser.flushInput()
        self.ser.write(b"AT+STATUS?\r\n")
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            for entry in self._read_bt_com_payloads():
                if "OK+CONN" in entry: return "CONNECTED"
                if "OK+UNCONN" in entry: return "DISCONNECTED"
            time.sleep(0.05)
        return "TIMEOUT"

    def reset(self):
        """Triggers AT+RESET and returns True if OK+RESET is received."""
        self.ser.write(b"AT+RESET\r\n")
        start_time = time.time()
        while (time.time() - start_time) < 5.0:
            for entry in self._read_bt_com_payloads():
                if "OK+RESET" in entry:
                    print("Waiting for ESP32 to reboot (10s)...")
                    time.sleep(10) 
                    return True
            time.sleep(0.05)
        return False

    def listen(self):
        """Returns a list of data payloads from BLE (ignores AT replies)."""
        logs = self._read_bt_com_payloads()
        return [l for l in logs if not (l.startswith("OK+") or l.startswith("ERROR+"))]

    def send(self, text):
        """Sends data to be forwarded to HM-10 via GATT."""
        if not text.endswith("\n"):
            text += "\n"
        self.ser.write(text.encode('utf-8'))
