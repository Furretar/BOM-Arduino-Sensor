import serial
import csv
from datetime import datetime
import time

port = 'COM9'               
baud = 115200               
output_file = 'bom_sensor_data.csv'

print(f"opening serial port {port} at {baud} baud")
try:
    ser = serial.Serial(port, baud, timeout=2)
    time.sleep(2)
except Exception as e:
    print(f"error opening serial port: {e}")
    exit(1)

print(f"saving to {output_file}")
print(f"Timestamp,Pressure(MMHg),Temperature(C),Humidity(%)")

with open(output_file, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['Timestamp', 'Pressure', 'Temperature', 'Humidity'])

    try:
        while True:
            if ser.in_waiting > 0:
                try:
                    raw = ser.readline()
                    line = raw.decode('utf-8', errors='replace').strip()
                    now = datetime.now()
                    timestamp = now.strftime('%Y-%m-%d %H:%M:%S.') + f"{now.microsecond // 1000:03d}"

                    parts = line.split(',')
                    if len(parts) != 3:
                        continue

                    pressure, temp, humidity = parts
                    writer.writerow([timestamp, pressure, temp, humidity])
                    f.flush()

                    print(f"{timestamp} | {pressure} | {temp} | {humidity}")

                except Exception as e:
                    print(f"error parsing line: {e}")
    except KeyboardInterrupt:
        print("\nstopping")
        ser.close()
