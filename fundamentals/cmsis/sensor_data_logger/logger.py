from datetime import datetime
import csv
import serial

PORT = "/dev/cu.usbserial-BH00M3NI"
BAUD_RATE = 115200
CSV_FILENAME = datetime.now().strftime(
    "photoresistor_%Y%m%d_%H%M%S_%f.csv"
)
HEADERS = ["Time (ms)", "ADC_Reading"]

try:
    with serial.Serial(PORT, BAUD_RATE, timeout=1) as port:
        with open(CSV_FILENAME, "x", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(HEADERS)
            while True:
                raw_line = port.readline()
                if raw_line:
                    try:
                        line = raw_line.decode("ascii").strip()
                    except UnicodeDecodeError:
                        continue
                    fields = line.split(",")
                    if len(fields) != 2:
                        continue

                    try:
                        timestamp_ms = int(fields[0])
                        adc_raw = int(fields[1])
                    except ValueError:
                        continue

                    if not (0 <= timestamp_ms <= 0xFFFFFFFF and 0 <= adc_raw <= 4095):
                        continue

                    writer.writerow([timestamp_ms, adc_raw])
                    file.flush()
                    print(timestamp_ms, adc_raw)
                else:
                    continue
except KeyboardInterrupt:
    print("\nLogger stopped.")
except serial.SerialException as error:
    print(f"\nSerial connection error: {error}")
except FileExistsError:
    print(f"\nCapture already exists: {CSV_FILENAME}")
except OSError as error:
    print(f"\nFile or operating-system error: {error}")


    
   
