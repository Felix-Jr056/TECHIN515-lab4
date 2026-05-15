"""
Process Gesture Data - Serial to CSV Converter (Dual MPU6050)

Reads dual-sensor accelerometer data from ESP32 and saves to CSV files.
Each row contains 6 axes: ax1,ay1,az1 (handle) + ax2,ay2,az2 (sheath).

Usage:
    python process_gesture_data.py [options]

Options:
    --port PORT       Serial port (default: auto-detect)
    --baud BAUD       Baud rate (default: 115200)
    --gesture NAME    Gesture label: pull_out | push_back | slash
    --person NAME     Your name (default: "user")
    --output DIR      Output directory (default: "data")
"""

import serial
import serial.tools.list_ports
import os
import time
import argparse
import csv
from datetime import datetime
import sys


def find_arduino_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if any(tag in p.description for tag in ["CP210", "CH340", "FTDI", "USB Serial"]):
            return p.device
    return None


def list_available_ports():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return "No serial ports found."
    result = "Available ports:\n"
    for i, p in enumerate(ports):
        result += f"{i+1}. {p.device} - {p.description}\n"
    return result


def ensure_directory(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)
        print(f"Created directory: {directory}")


def save_data_to_csv(filepath, data):
    """Save dual-sensor data: timestamp + 6 axes per row."""
    with open(filepath, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['timestamp', 'ax1', 'ay1', 'az1', 'ax2', 'ay2', 'az2'])
        for i, row in enumerate(data):
            writer.writerow([i * 10] + row)  # 10ms intervals → 100Hz
    print(f"Saved {len(data)} samples to {filepath}")
    return len(data)


def main():
    parser = argparse.ArgumentParser(description="Process dual-sensor gesture data from ESP32")
    parser.add_argument("--port", help="Serial port (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--gesture", default="gesture",
                        help="Gesture label: pull_out, push_back, slash (default: 'gesture')")
    parser.add_argument("--person", default="user")
    parser.add_argument("--output", default="data")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        print(list_available_ports())
        return

    port = args.port or find_arduino_port()
    if not port:
        print("Error: Could not auto-detect ESP32 port.")
        print(list_available_ports())
        print("Specify with --port")
        return

    output_dir = os.path.join(args.output, args.gesture)
    ensure_directory(output_dir)

    print(f"Connecting to ESP32 on {port} at {args.baud} baud...")
    print(f"Gesture label: '{args.gesture}'")
    print("Gestures: pull_out (draw knife), push_back (sheathe), slash")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
        time.sleep(2)

        print("Connected! Press 'o' to capture, 'p' to stop. Ctrl+C to quit.")

        collecting = False
        current_data = []
        capture_count = 0

        while True:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8').strip()

                    if "-,-,-,-,-,-" in line:
                        collecting = True
                        current_data = []
                        print("Capture started...")
                        continue

                    if "Capture complete" in line:
                        if current_data:
                            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                            capture_count += 1
                            filename = f"output_{args.gesture}_{args.person}_{capture_count}_{timestamp}.csv"
                            filepath = os.path.join(output_dir, filename)
                            save_data_to_csv(filepath, current_data)
                            current_data = []
                        else:
                            print("Warning: No data collected during this capture.")
                        collecting = False
                        continue

                    if collecting and "," in line:
                        try:
                            values = list(map(float, line.split(',')))
                            if len(values) == 6:
                                current_data.append(values)
                            else:
                                print(f"Skipping malformed line ({len(values)} values): {line}")
                        except ValueError:
                            pass

                except UnicodeDecodeError:
                    pass

            # Keyboard input
            if os.name == 'nt':
                import msvcrt
                if msvcrt.kbhit():
                    key = msvcrt.getch().decode('utf-8')
                    if key == 'o':
                        ser.write(b'o')
                        print("Sent start command...")
                    elif key == 'p':
                        ser.write(b'p')
                        print("Sent stop command...")
            else:
                import select
                if select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1)
                    if key == 'o':
                        ser.write(b'o')
                        print("Sent start command...")
                    elif key == 'p':
                        ser.write(b'p')
                        print("Sent stop command...")

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nExiting...")
    except serial.SerialException as e:
        print(f"Error: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial connection closed")


if __name__ == "__main__":
    try:
        import serial
    except ImportError:
        print("Error: pyserial not installed. Run: pip install pyserial")
        sys.exit(1)
    main()
