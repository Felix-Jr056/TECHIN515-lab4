// Dual LSM6DS3 gesture capture
// Handle sensor: SA0 → GND  → I2C address 0x6A
// Sheath sensor: SA0 → 3.3V → I2C address 0x6B

#include <Adafruit_LSM6DS3.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_LSM6DS3 imu_handle;  // 0x6A — in knife handle (SA0 → GND)
Adafruit_LSM6DS3 imu_sheath;  // 0x6B — in sheath      (SA0 → 3.3V)

long last_sample_millis = 0;
bool capture = false;
char cmd;
unsigned long capture_start_time = 0;
const unsigned long CAPTURE_DURATION = 1000;  // 1s at 100Hz = 100 samples

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    while (!imu_handle.begin_I2C(0x6A)) {
        Serial.println("Failed to find handle LSM6DS3 (0x6A) — check SA0 → GND");
        delay(500);
    }
    while (!imu_sheath.begin_I2C(0x6B)) {
        Serial.println("Failed to find sheath LSM6DS3 (0x6B) — check SA0 → 3.3V");
        delay(500);
    }

    imu_handle.setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
    imu_handle.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
    imu_handle.setAccelDataRate(LSM6DS_RATE_104_HZ);

    imu_sheath.setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
    imu_sheath.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
    imu_sheath.setAccelDataRate(LSM6DS_RATE_104_HZ);

    Serial.println("Both LSM6DS3 sensors initialized");
    Serial.println("Send 'o' to start capture, 'p' to stop");
    delay(100);
}

void capture_data() {
    if ((millis() - last_sample_millis) >= 10) {  // 100Hz
        last_sample_millis = millis();

        sensors_event_t a1, g1, t1;
        sensors_event_t a2, g2, t2;
        imu_handle.getEvent(&a1, &g1, &t1);
        imu_sheath.getEvent(&a2, &g2, &t2);

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
            Serial.print("-,-,-,-,-,-\n");
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
