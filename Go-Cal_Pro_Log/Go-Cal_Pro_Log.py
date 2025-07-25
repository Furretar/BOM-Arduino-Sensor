import serial, csv, re, time, os
from datetime import datetime, timedelta
from bisect import bisect_left
from serial.tools import list_ports

# — Auto find Arduino port —
FORCE_PORT = None  # e.g. "COM4"

def find_serial_port():
    if FORCE_PORT:
        try:
            test_ser = serial.Serial(FORCE_PORT)
            test_ser.close()
            print("Using forced port:", FORCE_PORT)
            return FORCE_PORT
        except serial.SerialException as e:
            raise SystemExit(f"Forced port {FORCE_PORT} failed to open: {e}")

    ports = list_ports.comports()
    arduinos = [p.device for p in ports if "Arduino" in p.description]
    candidate_ports = arduinos if arduinos else [p.device for p in ports]

    if not candidate_ports:
        raise SystemExit("No serial ports found!")

    print("Detected serial ports:", candidate_ports)

    valid_ports = []
    for p in candidate_ports:
        try:
            test_ser = serial.Serial(p)
            test_ser.close()
            valid_ports.append(p)
        except serial.SerialException:
            continue

    if not valid_ports:
        raise SystemExit("No usable serial ports available.")

    i = 0
    while True:
        port = valid_ports[i % len(valid_ports)]
        print(f"\nUsing port: {port}")
        print("Press Enter to try the next one, or Ctrl+C to quit.")
        input()
        i += 1

port = find_serial_port()
baud = 115200

# regex patterns
tera_path = "teraterm.log"
tera_pattern = re.compile(
    r"\[([0-9\-:\. ]+)\] \$DATA: \{.*?\"flow\":\"([\d\.]+)\".*?\"baro\":\"([\d\.]+)\"")
out_file = "bom_sensor_data.csv"

# prepare CSV 
first_time = not os.path.exists(out_file) or os.stat(out_file).st_size == 0
f_out = open(out_file, "a", newline="")
writer = csv.writer(f_out)
if first_time:
    writer.writerow([
        "Timestamp",
        "Pressure(mmHg)",
        "Temperature(C)",
        "Humidity(%)",
        "TeraFlow",
        "TeraPressure",
        "TeraTimestamp"
    ])

# open and seek TeraTerm log for the end
pump_f = open(tera_path, "r") if os.path.exists(tera_path) else None
tera_pos = pump_f.tell() if pump_f else 0

# In memory sorted list of tera entries
tera_entries = []
tera_times   = []

def reload_new_tera():
    """Read any new lines in teraterm.log and append to our lists."""
    global pump_f, tera_pos

    if not os.path.exists(tera_path):
        pump_f = None
        return

    if pump_f is None:
        pump_f = open(tera_path, "r")
        tera_pos = 0
        tera_entries.clear()
        tera_times.clear()

    pump_f.seek(tera_pos)

    for ln in pump_f:
        m = tera_pattern.search(ln)
        if not m:
            continue
        ts_str, flow, baro = m.groups()
        ts_str = " ".join(ts_str.split())
        try:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
        except ValueError:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")
        if tera_times and ts <= tera_times[-1]:
            continue
        tera_entries.append((ts, flow, baro))
        tera_times.append(ts)

    tera_pos = pump_f.tell()

# Open Arduino serial
ser = serial.Serial(port, baud, timeout=2)
time.sleep(2)

# Send time sync with seconds adjustment
adjusted_time = datetime.now() + timedelta(seconds=0)
time_sync_cmd = f"SETTIME {adjusted_time.strftime('%Y-%m-%d %H:%M:%S')}\n"
ser.write(time_sync_cmd.encode())

# Read Arduino reply (optional)
reply = ser.readline().decode().strip()
print(f"Sent time sync: {time_sync_cmd.strip()}")
print(f"Arduino replied: {reply}")

print(f"Logging to {out_file}, matching within 1 second to TeraTerm. Ctrl+C to quit.")
print(f"Arduino Timestamp   |Pressure(mmHg)|Temperature(C)|Humidity(%)|Pump Flow(mL/min)|Pump Pressure(mmHg)|Pump Timestamp")

try:
    while True:
        # 1) pull in any new TeraTerm lines
        reload_new_tera()

        # 2) read one Arduino line
        line = ser.readline().decode("utf-8", "ignore").strip()
        if "|" not in line:
            continue

        # 3) parse Arduino
        left, right = line.split("|", 1)
        ts_str = left.strip()
        vals   = [v.strip().split()[0] for v in right.split(",")]
        if len(vals) < 3:
            continue
        pressure, temp, hum = vals[:3]
        try:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
        except ValueError:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")

        # 4) find nearest TeraTerm entry within 1 second
        flow = baro = pump_ts_str = ""
        if tera_times:
            idx = bisect_left(tera_times, ts)
            for j in (idx-1, idx):
                if 0 <= j < len(tera_times):
                    delta = abs((tera_times[j] - ts).total_seconds())
                    if delta <= 1:
                        tts, flow, baro = tera_entries[j]
                        pump_ts_str = tts.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                        break

        # 5) write and flush
        writer.writerow([
            ts.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3],
            pressure, temp, hum,
            flow, baro, pump_ts_str
        ])
        f_out.flush()

        print(f"{ts_str}|{pressure}        |{temp}         |{hum}  "
              f"    |{flow}        |{baro}             |{pump_ts_str}")

except KeyboardInterrupt:
    print("\nStopping.")
finally:
    ser.close()
    f_out.close()
    if pump_f:
        pump_f.close()
