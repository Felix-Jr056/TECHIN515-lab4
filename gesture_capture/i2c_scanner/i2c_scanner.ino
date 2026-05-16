// I2C Pin Finder — tries all SDA/SCL combinations to locate MPU6050
#include <Wire.h>

// Candidate GPIO pins on XIAO ESP32C3
const int PINS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21};
const int N = sizeof(PINS) / sizeof(PINS[0]);

bool scanPair(int sda, int scl) {
    Wire.end();
    Wire.begin(sda, scl);
    delay(50);
    bool found = false;
    for (uint8_t addr : {0x68, 0x69, 0x6A, 0x6B}) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  → 0x"); Serial.print(addr, HEX);
            Serial.print(" found on SDA=GPIO"); Serial.print(sda);
            Serial.print(" SCL=GPIO"); Serial.println(scl);
            found = true;
        }
    }
    return found;
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("Searching for MPU6050 on all pin pairs...");

    bool anyFound = false;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i == j) continue;
            int sda = PINS[i], scl = PINS[j];
            Wire.end();
            Wire.begin(sda, scl);
            delay(30);
            Wire.beginTransmission(0x68);
            if (Wire.endTransmission() == 0) {
                Serial.print("MPU6050 (0x68) on SDA=GPIO");
                Serial.print(sda); Serial.print(" SCL=GPIO"); Serial.println(scl);
                anyFound = true;
            }
            Wire.beginTransmission(0x69);
            if (Wire.endTransmission() == 0) {
                Serial.print("MPU6050 (0x69) on SDA=GPIO");
                Serial.print(sda); Serial.print(" SCL=GPIO"); Serial.println(scl);
                anyFound = true;
            }
        }
    }

    if (!anyFound)
        Serial.println("MPU6050 not found on any pin pair — check power and SDA/SCL solder joints.");
    else
        Serial.println("Done.");
}

void loop() {}
