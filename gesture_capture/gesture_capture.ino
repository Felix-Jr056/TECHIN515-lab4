// Dual MPU6050 gesture capture for knife wand
// Handle sensor: AD0 → GND  → I2C address 0x68
// Sheath sensor: AD0 → 3.3V → I2C address 0x69

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu_handle;  // 0x68 — mounted in knife handle
Adafruit_MPU6050 mpu_sheath;  // 0x69 — mounted in sheath

long last_sample_millis = 0;
bool capture = false;
char cmd;
unsigned long capture_start_time = 0;
const unsigned long CAPTURE_DURATION = 1000;  // 1s at 100Hz = 100 samples

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10);

  while (!mpu_handle.begin(0x68)) {
    Serial.println("Failed to find handle MPU6050 (0x68) — check AD0 wiring");
    delay(500);
  }
  while (!mpu_sheath.begin(0x69)) {
    Serial.println("Failed to find sheath MPU6050 (0x69) — check AD0 wiring");
    delay(500);
  }

  mpu_handle.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu_handle.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu_handle.setFilterBandwidth(MPU6050_BAND_21_HZ);

  mpu_sheath.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu_sheath.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu_sheath.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Both MPU6050 sensors initialized");
  Serial.println("Send 'o' to start capture, 'p' to stop");
  delay(100);
}

void capture_data() {
  if ((millis() - last_sample_millis) >= 10) {  // 100Hz
    last_sample_millis = millis();

    sensors_event_t a1, g1, t1;
    sensors_event_t a2, g2, t2;
    mpu_handle.getEvent(&a1, &g1, &t1);
    mpu_sheath.getEvent(&a2, &g2, &t2);

    // Format: ax1,ay1,az1,ax2,ay2,az2
    Serial.print(a1.acceleration.x); Serial.print(",");
    Serial.print(a1.acceleration.y); Serial.print(",");
    Serial.print(a1.acceleration.z); Serial.print(",");
    Serial.print(a2.acceleration.x); Serial.print(",");
    Serial.print(a2.acceleration.y); Serial.print(",");
    Serial.print(a2.acceleration.z); Serial.print("\n");

    if (millis() - capture_start_time >= CAPTURE_DURATION) {
      capture = false;
      Serial.print("\n\n\n\n");
      Serial.println("Capture complete (1 second)");
    }
  }
}

void loop() {
  if (Serial.available() > 0) {
    cmd = Serial.read();
    if (cmd == 'o') {
      Serial.print("-,-,-,-,-,-\n");  // start marker (6 dashes for 6 axes)
      capture = true;
      capture_start_time = millis();
      last_sample_millis = millis();
      Serial.println("Starting capture...");
    } else if (cmd == 'p') {
      capture = false;
      Serial.print("\n\n\n\n");
      Serial.println("Capture stopped manually");
    }
  }
  if (capture) capture_data();
}
