import serial
import time
import re

class HM10ESP32Bridge:
    """
    A robust bridge for communicating with an HM-10 Bluetooth module via an ESP32.
    Enhanced with line-sync and priority handling to prevent data corruption.
    """
    def __init__(self, port, rx_timeout=0.1):
        self.ser = serial.Serial(port=port, baudrate=115200, timeout=rx_timeout)
        # Matches 'bt_com' tag logs from ESP32
        self.log_regex = re.compile(r'bt_com:\s*(.*)')
        # Strips ANSI color codes often sent by ESP-IDF
        self.ansi_regex = re.compile(r'\x1b\[[0-9;]*m')
        self.msg_accumulator = ""
        self.raw_accumulator = ""
        time.sleep(1) 

    def flush(self):
        """Clears serial buffers and internal accumulators to sync with fresh data."""
        self.ser.read_all()
        self.msg_accumulator = ""
        self.raw_accumulator = ""

    def _read_bt_com_payloads(self):
        """
        Reads logs and stitches fragments into complete messages.
        Uses robust line splitting and avoids aggressive stripping to prevent merging.
        """
        if self.ser.in_waiting > 0:
            try:
                self.raw_accumulator += self.ser.read_all().decode('utf-8', errors='ignore')
            except Exception:
                pass
            
        if not self.raw_accumulator:
            return []

        complete_messages = []
        # 1. Process complete lines from the ESP32 log stream
        while '\n' in self.raw_accumulator or '\r' in self.raw_accumulator:
            m = re.search(r'[\r\n]', self.raw_accumulator)
            if not m: break
            
            line = self.raw_accumulator[:m.start()]
            self.raw_accumulator = self.raw_accumulator[m.end():]
            
            # Handle \r\n as a single break
            if m.group() == '\r' and self.raw_accumulator.startswith('\n'):
                self.raw_accumulator = self.raw_accumulator[1:]
            
            if not line.strip(): continue
            
            match = self.log_regex.search(line)
            if match:
                # Clean ANSI colors but preserve terminators if present
                payload = self.ansi_regex.sub('', match.group(1))

                # PRIORITY MESSAGES: Handle RFID and reqNxtMove immediately
                if "reqNxtMove" in payload or "RFID:" in payload:
                    if "reqNxtMove" in payload:
                        complete_messages.append("reqNxtMove")
                        payload = payload.replace("reqNxtMove", "")
                    
                    if "RFID:" in payload:
                        rfid_match = re.search(r'RFID:[a-zA-Z0-9]+', payload)
                        if rfid_match:
                            complete_messages.append(rfid_match.group())
                            payload = payload.replace(rfid_match.group(), "")

                # AT COMMAND RESPONSES: Always bypass sync logic
                if payload.startswith("OK+") or payload.startswith("ERROR+"):
                    complete_messages.append(payload.strip())
                    continue

                # Add to data accumulator
                if payload.strip():
                    self.msg_accumulator += payload
                    # Add newline if it's a complete log line to prevent gluing
                    if not payload.endswith('\n') and not payload.endswith('\r'):
                        self.msg_accumulator += '\n'

        # 2. Extract complete messages from the data accumulator
        if '\n' in self.msg_accumulator or '\r' in self.msg_accumulator:
            parts = re.split(r'[\r\n]+', self.msg_accumulator)
            
            if self.msg_accumulator and self.msg_accumulator[-1] in "\r\n":
                for p in parts:
                    if p.strip(): complete_messages.append(p.strip())
                self.msg_accumulator = ""
            else:
                for p in parts[:-1]:
                    if p.strip(): complete_messages.append(p.strip())
                self.msg_accumulator = parts[-1]

        # Comma-based fallback: handle multi-packet chunks
        while self.msg_accumulator.count(',') >= 8:
            parts = self.msg_accumulator.split(',')
            if len(parts) >= 9:
                complete_messages.append(",".join(parts[:9]).strip())
                self.msg_accumulator = ",".join(parts[9:])
            else:
                break

        # Safety flush
        if len(self.msg_accumulator) > 1000:
            self.msg_accumulator = ""

        return complete_messages

    def set_hm10_name(self, name, timeout=3.0):
        """Sends AT+NAME<name> and verifies OK+SET reply."""
        self.ser.flushInput()
        self.ser.write(f"AT+NAME{name}\r\n".encode('utf-8'))
        pattern = re.compile(fr"OK\+SET[:\s]*{re.escape(name)}")
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            for entry in self._read_bt_com_payloads():
                if pattern.search(entry): return True
            time.sleep(0.05)
        return False

    def get_hm10_name(self, timeout=3.0):
        """Queries the device name with retries."""
        for attempt in range(2):
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
        """Triggers AT+RESET."""
        self.ser.write(b"AT+RESET\r\n")
        start_time = time.time()
        while (time.time() - start_time) < 5.0:
            for entry in self._read_bt_com_payloads():
                if "OK+RESET" in entry:
                    time.sleep(10) 
                    return True
            time.sleep(0.05)
        return False

    def listen(self):
        """Returns only non-AT payloads."""
        logs = self._read_bt_com_payloads()
        return [l for l in logs if not (l.startswith("OK+") or l.startswith("ERROR+"))]

    def send(self, text):
        """Sends data with mandatory newline."""
        if not text.endswith("\n"):
            text += "\n"
        self.ser.write(text.encode('utf-8'))
        time.sleep(0.05)
