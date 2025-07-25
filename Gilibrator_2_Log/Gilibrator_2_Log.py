import serial, csv, re, time, os
from datetime import datetime, timedelta
from serial.tools import list_ports

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

    for p in candidate_ports:
        try:
            test_ser = serial.Serial(p)
            test_ser.close()
        except serial.SerialException:
            continue

        while True:
            print(f"\nFound working port: {p}")
            user_input = input("Press Enter to skip, or type 'y' to use this port: ").strip().lower()
            if user_input == "y":
                print("Using port:", p)
                return p
            elif user_input == "":
                break

    raise SystemExit("No usable serial ports selected.")

port = find_serial_port()
baud = 115200

tera_path = "teraterm.log"
tera_triplet_pattern = re.compile(r"(\d+(?:\.\d+)?)\s+(\d+(?:\.\d+)?)\s+(\d+(?:\.\d+)?)")


out_file = "bom_sensor_data.csv"

first_time = not os.path.exists(out_file) or os.stat(out_file).st_size == 0
f_out = open(out_file, "a", newline="")
writer = csv.writer(f_out)
if first_time:
    writer.writerow([
        "Timestamp",
        "Pressure(mmHg)",
        "Temperature(C)",
        "Humidity(%)",
        "Flow",
        "AverageFlow",
        "SampleNumber"
    ])

tera_entries = []
tera_pos = 0
pump_f = None

def reload_new_tera():
    global pump_f, tera_pos, tera_entries, tera_path

    if not os.path.exists(tera_path):
        pump_f = None
        return

    try:
        stat = os.stat(tera_path)
        current_size = stat.st_size
        current_mtime = stat.st_mtime
    except Exception:
        return

    if pump_f is None or pump_f.closed:
        pump_f = open(tera_path, "r")
        tera_pos = 0
        tera_entries.clear()

    elif tera_pos > current_size:
        print("TeraTerm log was truncated or rotated. Reopening.")
        pump_f.close()
        pump_f = open(tera_path, "r")
        tera_pos = 0
        tera_entries.clear()

    pump_f.seek(tera_pos)
    new_data = pump_f.read()
    tera_pos = pump_f.tell()

    if not new_data.strip():
        return

    # print(f"Detected new TeraTerm log data: {new_data}")

    matches = tera_triplet_pattern.findall(new_data)
    # print("Detected new TeraTerm log data joined:", ' '.join(f"{a} {b} {c}" for a, b, c in matches))
    for flow, avg_flow, sample_str in matches:
        sample_num = int(sample_str)
        flow_val = float(flow)
        avg_flow_val = float(avg_flow)
        if tera_entries and sample_num <= tera_entries[-1][0]:
            continue
        tera_entries.append((sample_num, flow_val, avg_flow_val))




ser = serial.Serial(port, baud, timeout=2)
time.sleep(2)

adjusted_time = datetime.now() + timedelta(seconds=0)
time_sync_cmd = f"SETTIME {adjusted_time.strftime('%Y-%m-%d %H:%M:%S')}\n"
ser.write(time_sync_cmd.encode())

reply = ser.readline().decode().strip()
print(f"Sent time sync: {time_sync_cmd.strip()}")
print(f"Arduino replied: {reply}")

print(f"Logging to {out_file}, showing latest flow from TeraTerm. Ctrl+C to quit.")
print(f"Arduino Timestamp   |Pressure(mmHg)|Temperature(C)|Humidity(%)|Flow        |AverageFlow    |SampleNumber")

# On startup, ignore printing any existing sample by initializing to highest sample present
last_sample_num_printed = None

# Preload existing tera entries once to get current highest sample number
if os.path.exists(tera_path):
    with open(tera_path, "r") as f:
        content = f.read()
    matches = tera_triplet_pattern.findall(content)
    for flow, avg_flow, sample_str in matches:
        sample_num = int(sample_str)
        flow_val = float(flow)
        avg_flow_val = float(avg_flow)
        if tera_entries and sample_num <= tera_entries[-1][0]:
            continue
        tera_entries.append((sample_num, flow_val, avg_flow_val))

if tera_entries:
    last_sample_num_printed = tera_entries[-1][0]  # ignore printing initial last entry


try:
    while True:
        reload_new_tera()

        line = ser.readline().decode("utf-8", "ignore").strip()
        if "|" not in line:
            continue

        left, right = line.split("|", 1)
        ts_str = left.strip()
        vals = [v.strip().split()[0] for v in right.split(",")]
        if len(vals) < 3:
            continue
        pressure, temp, hum = vals[:3]
        try:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
        except ValueError:
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")

        flow = avg_flow = sample_num = ""
        if tera_entries:
            sample_num, flow_val, avg_flow_val = tera_entries[-1]
            flow = str(flow_val)
            avg_flow = str(avg_flow_val)

        writer.writerow([
            ts.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3],
            pressure, temp, hum,
            flow, avg_flow, sample_num
        ])
        f_out.flush()

        if sample_num != last_sample_num_printed:
            print(f"{ts_str}|{pressure}        |{temp}         |{hum}  "
                  f"    |{flow}        |{avg_flow}              |{sample_num}")
            last_sample_num_printed = sample_num

except KeyboardInterrupt:
    print("\nStopping.")
finally:
    ser.close()
    f_out.close()
    if pump_f:
        pump_f.close()
