"""
Dual MPU6050 Gesture Data Capture — Auto Countdown Mode

Usage:
    python process_gesture_data.py --gesture pull_out --person felix
    python process_gesture_data.py --gesture push_back --person felix --port /dev/cu.usbmodem101
    python process_gesture_data.py --gesture slash    --person felix --samples 30
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
        if any(kw in p.description for kw in ["CP210", "CH340", "FTDI", "USB Serial", "JTAG", "usbmodem"]):
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
            writer.writerow([i * 10] + row)  # 10ms intervals = 100Hz
    print(f"  Saved {len(data)} samples → {filepath}")
    return len(data)


def main():
    parser = argparse.ArgumentParser(description="Auto-countdown dual MPU6050 gesture capture")
    parser.add_argument("--port",    help="Serial port (default: auto-detect)")
    parser.add_argument("--baud",    type=int, default=115200)
    parser.add_argument("--gesture", default="gesture",
                        help="Gesture label: pull_out | push_back | slash")
    parser.add_argument("--person",  default="user")
    parser.add_argument("--output",  default="data")
    parser.add_argument("--samples", type=int, default=20,
                        help="Number of captures to collect (default: 20)")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()

    if args.list_ports:
        print(list_available_ports())
        return

    port = args.port or find_arduino_port()
    if not port:
        print("Error: Could not auto-detect port.")
        print(list_available_ports())
        print("Specify with --port")
        return

    output_dir = os.path.join(args.output, args.gesture)
    ensure_directory(output_dir)

    print(f"Connecting on {port} at {args.baud} baud...")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
        time.sleep(5)
        ser.reset_input_buffer()

        target = args.samples
        print(f"Connected! Will collect {target} samples for gesture '{args.gesture}'.")
        print("Press Ctrl+C to stop early.\n")

        capture_count = 0

        while capture_count < target:
            print(f"[{capture_count}/{target}] Get ready — gesture: '{args.gesture}'")
            for i in range(3, 0, -1):
                print(f"  {i}...")
                time.sleep(1)
            print("  GO! Do the gesture now.")

            ser.reset_input_buffer()
            ser.write(b'o')
            ser.flush()

            collecting = False
            current_data = []
            deadline = time.time() + 8

            while time.time() < deadline:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                except Exception:
                    continue
                if not line:
                    continue
                print(f"  [rx] {repr(line)}")  # debug: show raw serial output
                if "-,-,-,-,-,-" in line:
                    collecting = True
                    continue
                if "Capture complete" in line:
                    break
                if collecting and "," in line:
                    try:
                        values = list(map(float, line.split(',')))
                        if len(values) == 6:
                            current_data.append(values)
                    except ValueError:
                        pass

            if current_data:
                capture_count += 1
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"output_{args.gesture}_{args.person}_{capture_count}_{timestamp}.csv"
                filepath = os.path.join(output_dir, filename)
                save_data_to_csv(filepath, current_data)
                print()
            else:
                print("  No data received, retrying...\n")

        print(f"\nDone! Collected {capture_count} samples for '{args.gesture}'.")

    except KeyboardInterrupt:
        print("\nStopped early.")
    except serial.SerialException as e:
        print(f"Serial error: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Port closed.")


if __name__ == "__main__":
    try:
        import serial
    except ImportError:
        print("Error: pyserial not installed. Run: pip install pyserial")
        sys.exit(1)
    main()
