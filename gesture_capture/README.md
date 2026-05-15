# TECHIN515 Lab4 - Gesture Data Capture

This directory contains tools for capturing accelerometer data from an ESP32 with MPU6050 for gesture recognition.

## Contents

- `gesture_capture.ino`: Arduino sketch for the ESP32 that captures accelerometer data
- `process_gesture_data.py`: Python script to automatically save gesture data to CSV files
- `requirements.txt`: Python dependencies

## Setup

### Hardware Setup

1. Connect the MPU6050 to your ESP32:
   - VCC → 3.3V
   - GND → GND
   - SCL → GPIO22 (or your I2C clock pin)
   - SDA → GPIO21 (or your I2C data pin)

**Note**: The PIN number may vary depending on your developmen board.

### Arduino Setup

1. Install the required libraries in Arduino IDE:
   - Adafruit MPU6050
   - Adafruit Sensor
   - Wire (built-in)

2. Upload the `gesture_capture.ino` sketch to your ESP32

### Python Setup

1. Install Python 3.6 or newer
2. Install the required Python package:
   
   ```
   pip install -r requirements.txt
   ```

## Usage

### Data Collection

1. Connect your ESP32 to your computer
2. Run the Python script:
   ```bash
   pytho3 process_gesture_data.py --gesture "W" --person "your_name"
   ```
   
3. The script will:
   - Auto-detect your ESP32's serial port
   - Create a directory structure for your data
   - Wait for you to press 'o' to start a capture
   - Automatically save each gesture capture to a CSV file

4. To capture a gesture:
   - Type 'o' and press Enter to start recording
   - Perform your gesture (recording will stop automatically after 1 second)
   - Repeat for multiple samples

5. Press Ctrl+C to exit the script when done

### Command Line Options

```bash
python3 process_gesture_data.py [options]

Options:
  --port PORT       Serial port to use (default: auto-detect)
  --baud BAUD       Baud rate (default: 115200)
  --gesture NAME    Gesture name (default: "gesture")
  --person NAME     Person name (default: "user")
  --output DIR      Output directory (default: "data")
```

## Data Format

The CSV files contain four columns:

- Timestamp
- x: X-axis acceleration (in m/s²)
- y: Y-axis acceleration (in m/s²)
- z: Z-axis acceleration (in m/s²)

Each file contains approximately 100 samples (at 100Hz for 1 second).

## Troubleshooting

- **Port not found**: Specify the port manually with `--port COM3` (Windows) or `--port /dev/ttyUSB0` (Linux/Mac)
- **Permission denied**: On Linux/Mac, you may need to run `sudo chmod 666 /dev/ttyUSB0` (replace with your port)
- **No data received**: Make sure the ESP32 is properly connected and the sketch is uploaded correctly 