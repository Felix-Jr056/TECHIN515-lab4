/* Knife Wand — Dual MPU6050 Gesture Inference
 * Handle sensor: AD0 → GND  → 0x68
 * Sheath sensor: AD0 → 3.3V → 0x69
 *
 * Gestures:
 *   pull_out   → 拔剑 — Expecto Patronum (Blue)
 *   push_back  → 合剑 — Episkey           (Green)
 *   slash      → 挥动 — Sectumsempra      (Red)
 *
 * Wiring (XIAO ESP32S3):
 *   Button      → D1 (INPUT_PULLUP, active LOW)
 *   NeoPixel    → D0
 *   MPU SDA     → D4
 *   MPU SCL     → D5
 */

#include <felix_wand_inferencing.h>
#include <Adafruit_LSM6DS3.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define BUTTON_PIN      D1
#define NEOPIXEL_PIN    D0
#define NEOPIXEL_COUNT  8

// ── Sensor objects ────────────────────────────────────────────────────────────
Adafruit_LSM6DS3   imu_handle;  // 0x6A — in knife handle (SA0 → GND)
Adafruit_LSM6DS3   imu_sheath;  // 0x6B — in sheath      (SA0 → 3.3V)
Adafruit_NeoPixel  strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ── Sampling config ───────────────────────────────────────────────────────────
#define SAMPLE_RATE_MS      10    // 100 Hz
#define CAPTURE_DURATION_MS 1000  // 1s window
#define AXES_PER_SAMPLE     6     // ax1,ay1,az1,ax2,ay2,az2
#define CONFIDENCE_THRESHOLD 0.7f

// ── State ─────────────────────────────────────────────────────────────────────
bool capturing = false;
unsigned long last_sample_time = 0;
unsigned long capture_start_time = 0;
int sample_count = 0;
bool button_last = HIGH;

float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// ── LED helpers ───────────────────────────────────────────────────────────────
void led_set(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NEOPIXEL_COUNT; i++)
        strip.setPixelColor(i, strip.Color(r, g, b));
    strip.show();
}

void led_rainbow() {
    uint8_t hue = (millis() / 3) & 0xFF;
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        uint8_t h = (hue + i * (256 / NEOPIXEL_COUNT)) & 0xFF;
        uint8_t r, g, b;
        if      (h < 85)  { r = h*3;       g = 255-h*3; b = 0;       }
        else if (h < 170) { h-=85; r = 255-h*3; g = 0;  b = h*3;     }
        else              { h-=170; r = 0;  g = h*3;     b = 255-h*3; }
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
}

// pull_out — Expecto Patronum: blue sweep tip-to-end, hold, fade out
void led_expecto() {
    // Sweep: light pixels one by one
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 80, 255));
        strip.show();
        delay(60);
    }
    delay(1500);
    // Fade out
    for (int b = 220; b >= 0; b -= 8) {
        led_set(0, 0, b);
        delay(20);
    }
    led_set(0, 0, 0);
}

// push_back — Episkey: green breathe (heal pulse x3)
void led_episkey() {
    for (int rep = 0; rep < 3; rep++) {
        for (int b = 0; b <= 200; b += 5) { led_set(0, b, 0); delay(12); }
        for (int b = 200; b >= 0; b -= 5) { led_set(0, b, 0); delay(12); }
    }
    led_set(0, 0, 0);
}

// slash — Sectumsempra: 3 sharp red flashes then hold red, fade
void led_sectumsempra() {
    for (int f = 0; f < 3; f++) {
        led_set(255, 0, 0); delay(80);
        led_set(0, 0, 0);   delay(60);
    }
    led_set(220, 0, 0);
    delay(1000);
    for (int r = 220; r >= 0; r -= 8) { led_set(r, 0, 0); delay(20); }
    led_set(0, 0, 0);
}

// ── Gesture → color mapping ───────────────────────────────────────────────────
struct Gesture {
    const char* label;
    const char* name;
    uint8_t r, g, b;
};

const Gesture GESTURES[] = {
    { "pull_out",  "Expecto Patronum — 拔剑", 0,   0,   220 },  // Blue
    { "push_back", "Episkey          — 合剑", 0,   200, 0   },  // Green
    { "slash",     "Sectumsempra     — 挥动", 220, 0,   0   },  // Red
};
const int NUM_GESTURES = sizeof(GESTURES) / sizeof(GESTURES[0]);

// ── Edge Impulse glue ─────────────────────────────────────────────────────────
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    strip.begin();
    strip.setBrightness(200);
    led_set(0, 0, 0);

    Serial.println("Initializing MPU6050 sensors...");

    while (!imu_handle.begin_I2C(0x6A)) {
        Serial.println("Handle LSM6DS3 (0x6A) not found — check SA0 → GND");
        delay(500);
    }
    while (!imu_sheath.begin_I2C(0x6B)) {
        Serial.println("Sheath LSM6DS3 (0x6B) not found — check SA0 → 3.3V");
        delay(500);
    }

    imu_handle.setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
    imu_handle.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
    imu_handle.setAccelDataRate(LSM6DS_RATE_104_HZ);

    imu_sheath.setAccelRange(LSM6DS_ACCEL_RANGE_8_G);
    imu_sheath.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
    imu_sheath.setAccelDataRate(LSM6DS_RATE_104_HZ);

    Serial.println("Both sensors ready. Press button (D1) or send 'o' to cast.");

    // Ready flash: blue
    led_set(0, 0, 180);
    delay(300);
    led_set(0, 0, 0);
}

// ── Capture ───────────────────────────────────────────────────────────────────
void start_capture() {
    Serial.println("Capturing...");
    sample_count = 0;
    capturing = true;
    capture_start_time = millis();
    last_sample_time = millis();
}

void capture_sensor_data() {
    if (millis() - last_sample_time < SAMPLE_RATE_MS) return;
    last_sample_time = millis();

    int max_samples = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXES_PER_SAMPLE;
    if (sample_count >= max_samples) return;

    sensors_event_t a1, g1, t1, a2, g2, t2;
    imu_handle.getEvent(&a1, &g1, &t1);
    imu_sheath.getEvent(&a2, &g2, &t2);

    int idx = sample_count * AXES_PER_SAMPLE;
    features[idx + 0] = a1.acceleration.x;
    features[idx + 1] = a1.acceleration.y;
    features[idx + 2] = a1.acceleration.z;
    features[idx + 3] = a2.acceleration.x;
    features[idx + 4] = a2.acceleration.y;
    features[idx + 5] = a2.acceleration.z;
    sample_count++;

    if (millis() - capture_start_time >= CAPTURE_DURATION_MS) {
        capturing = false;
        Serial.println("Capture complete — running inference...");
        run_inference();
    }
}

// ── Inference ─────────────────────────────────────────────────────────────────
void run_inference() {
    int expected = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXES_PER_SAMPLE;
    if (sample_count < expected) {
        Serial.println("Not enough samples.");
        return;
    }

    signal_t sig;
    sig.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    sig.get_data = &raw_feature_get_data;

    ei_impulse_result_t result = { 0 };
    if (run_classifier(&sig, &result, false) != EI_IMPULSE_OK) {
        Serial.println("Classifier error.");
        return;
    }

    float max_val = 0;
    int max_idx = -1;
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > max_val) {
            max_val = result.classification[i].value;
            max_idx = i;
        }
    }

    if (max_idx == -1 || max_val < CONFIDENCE_THRESHOLD) {
        Serial.println(">>> UNCERTAIN");
        led_set(80, 80, 80);
        delay(1500);
        led_set(0, 0, 0);
        return;
    }

    const char* predicted = ei_classifier_inferencing_categories[max_idx];
    Serial.print(">>> ");
    Serial.print(predicted);
    Serial.print(" (");
    Serial.print(max_val * 100, 1);
    Serial.println("%)");

    if (strcmp(predicted, "pull_out") == 0) {
        Serial.println("Expecto Patronum — 拔剑");
        led_expecto();
    } else if (strcmp(predicted, "push_back") == 0) {
        Serial.println("Episkey          — 合剑");
        led_episkey();
    } else if (strcmp(predicted, "slash") == 0) {
        Serial.println("Sectumsempra     — 挥动");
        led_sectumsempra();
    }
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    bool button_now = digitalRead(BUTTON_PIN);
    if (button_now == LOW && button_last == HIGH && !capturing) {
        start_capture();
    }
    button_last = button_now;

    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'o' && !capturing) start_capture();
    }

    if (capturing) {
        led_rainbow();
        capture_sensor_data();
    }
}
