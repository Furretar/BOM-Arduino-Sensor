import serial
import csv
import re
import time
from datetime import datetime
from serial.tools import list_ports
import os

# --- auto‑find Arduino port ---
ports = list(list_ports.comports())
arduino_ports = [p.device for p in ports if "Arduino" in p.description]
if arduino_ports:
    port = arduino_ports[0]
else:
    if not ports:
        print("No serial ports found!")
        exit(1)
    port = ports[0].device

baud = 115200
output_file = 'bom_sensor_data.csv'

print(f"Opening serial port {port} at {baud} baud…")
try:
    ser = serial.Serial(port, baud, timeout=2)
    time.sleep(2)  # let it warm up
except Exception as e:
    print(f"Error opening serial port: {e}")
    exit(1)

print(f"Logging to {output_file}")
print("Timestamp,Pressure(mmHg),Temperature(C),Humidity(%)")

# regex to pull three floats
pattern = re.compile(r"([-+]?\d*\.\d+|\d+)\s*mmHg,\s*([-+]?\d*\.\d+|\d+)\s*C,\s*([-+]?\d*\.\d+|\d+)\s*%")

file_exists = os.path.exists(output_file)
with open(output_file, 'a', newline='') as f:
    writer = csv.writer(f)
    if not file_exists or os.stat(output_file).st_size == 0:
        writer.writerow(['Timestamp','Pressure (MMHg)','Temperature (C)','Humidity (%)'])


    try:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            # look for our three values
            m = pattern.search(line)
            if not m:
                continue

            pressure, temp, humidity = m.groups()
            now = datetime.now()
            timestamp = now.strftime('%Y-%m-%d %H:%M:%S.') + f"{now.microsecond//1000:03d}"

            writer.writerow([timestamp, pressure, temp, humidity])
            f.flush()

            print(f"{timestamp} | {pressure} | {temp} | {humidity}")

    except KeyboardInterrupt:
        print("\nStopping.")
        ser.close()
