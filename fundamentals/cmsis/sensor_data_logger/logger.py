# Author: Pedro Valle-Cruz
# Capture complete, validated STM32 serial records into a new CSV per run.

from datetime import datetime
import csv
import serial

PORT = "/dev/cu.usbserial-BH00M3NI"
BAUD_RATE = 115200
# Relative filenames are created in the directory from which Python is run.
CSV_FILENAME = datetime.now().strftime(
    "photoresistor_%Y%m%d_%H%M%S_%f.csv"
)
HEADERS = ["Time (ms)", "ADC_Reading"]
# Firmware rows need at most 17 bytes; cap malformed input with room to spare.
MAX_LINE_BYTES = 64

try:
    with serial.Serial(PORT, BAUD_RATE, timeout=1) as port:
        # Exclusive creation protects previous captures; context managers close resources.
        with open(CSV_FILENAME, "x", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(HEADERS)
            pending = bytearray()
            # Synchronize at the first newline, even if the first record was complete.
            discarding = True
            while True:
                # Bound each read; a timeout can return a fragment or no bytes.
                chunk = port.readline(MAX_LINE_BYTES)
                if not chunk:
                    continue

                # Skip startup fragments or the remainder of an oversized record.
                if discarding:
                    if chunk.endswith(b"\n"):
                        discarding = False
                    continue

                # Preserve partial records across reads and empty timeouts.
                pending.extend(chunk)

                if len(pending) > MAX_LINE_BYTES:
                    pending.clear()
                    # If the newline is already present, the next read starts fresh.
                    discarding = not chunk.endswith(b"\n")
                    continue

                if not pending.endswith(b"\n"):
                    continue

                # Detach a complete record before validation so rejection cannot leak bytes.
                raw_line = bytes(pending)
                pending.clear()

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

                # Match the firmware uint32_t timestamp and 12-bit ADC range.
                if not (0 <= timestamp_ms <= 0xFFFFFFFF and 0 <= adc_raw <= 4095):
                    continue

                writer.writerow([timestamp_ms, adc_raw])
                # Flush Python buffering; this is not a disk durability guarantee.
                file.flush()
                print(timestamp_ms, adc_raw)

except KeyboardInterrupt:
    print("\nLogger stopped.")
# Specific OS-derived errors must precede the general OSError handler.
except serial.SerialException as error:
    print(f"\nSerial connection error: {error}")
except FileExistsError:
    print(f"\nCapture already exists: {CSV_FILENAME}")
except OSError as error:
    print(f"\nFile or operating-system error: {error}")
