/* Knife Wand — Dual MPU6050 Gesture Inference
 * Handle sensor: AD0 → GND  → 0x68
 * Sheath sensor: AD0 → 3.3V → 0x69
 *
 * Gestures:
 *   pull_out   → Expecto Patronum (Reflect Shield, 2x damage, 2MP)
 *   push_back  → Episkey           (Heal 1HP, 2MP)
 *   slash      → Sectumsempra      (Deal 1HP, 1MP)
 *
 * Wiring:
 *   Button      → GPIO 15 (pull-down: other leg to GND)
 *   LED Red     → GPIO 25 (Sectumsempra)
 *   LED Green   → GPIO 26 (Episkey)
 *   LED Blue    → GPIO 27 (Expecto Patronum)
 */

#include <your_edge_impulse_project_inferencing.h>  // rename to match your download
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
#define BUTTON_PIN  15
#define LED_RED     25   // Sectumsempra
#define LED_GREEN   26   // Episkey
#define LED_BLUE    27   // Expecto Patronum

// ── Sensor objects ───────────────────────────────────────────────────────────
Adafruit_MPU6050 mpu_handle;  // 0x68 — in knife handle
Adafruit_MPU6050 mpu_sheath;  // 0x69 — in sheath

// ── Sampling config ──────────────────────────────────────────────────────────
#define SAMPLE_RATE_MS      10    // 100Hz
#define CAPTURE_DURATION_MS 1000  // 1s window — matches capture script
#define AXES_PER_SAMPLE     6     // ax1,ay1,az1,ax2,ay2,az2

// ── State ────────────────────────────────────────────────────────────────────
bool capturing = false;
unsigned long last_sample_time = 0;
unsigned long capture_start_time = 0;
int sample_count = 0;

float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// ── Spell metadata ───────────────────────────────────────────────────────────
struct Spell {
    const char* label;      // must match Edge Impulse label
    const char* name;
    const char* effect;
    int led_pin;
};

const Spell SPELLS[] = {
    { "expecto_patronum", "Expecto Patronum", "Reflect Shield — 2x damage, costs 2MP", LED_BLUE  },
    { "episkey",          "Episkey",          "Heal 1HP, costs 2MP",                   LED_GREEN },
    { "sectumsempra",     "Sectumsempra",     "Deal 1HP, costs 1MP",                   LED_RED   },
};
const int NUM_SPELLS = sizeof(SPELLS) / sizeof(SPELLS[0]);

// ── Button debounce ──────────────────────────────────────────────────────────
bool last_button_state = LOW;
unsigned long last_debounce_time = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// ────────────────────────────────────────────────────────────────────────────

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void all_leds_off() {
    digitalWrite(LED_RED,   LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE,  LOW);
}

void flash_led(int pin, int times = 3, int on_ms = 200, int off_ms = 100) {
    for (int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(on_ms);
        digitalWrite(pin, LOW);
        delay(off_ms);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_RED,    OUTPUT);
    pinMode(LED_GREEN,  OUTPUT);
    pinMode(LED_BLUE,   OUTPUT);
    all_leds_off();

    Serial.println("Initializing MPU6050 sensors...");

    while (!mpu_handle.begin(0x68)) {
        Serial.println("Handle MPU6050 (0x68) not found — check AD0 wiring");
        delay(500);
    }
    while (!mpu_sheath.begin(0x69)) {
        Serial.println("Sheath MPU6050 (0x69) not found — check AD0 wiring");
        delay(500);
    }

    mpu_handle.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu_handle.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_handle.setFilterBandwidth(MPU6050_BAND_21_HZ);

    mpu_sheath.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu_sheath.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_sheath.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("Both sensors initialized. Press button to cast a spell.");

    // Ready signal: all LEDs blink once
    for (int pin : {LED_RED, LED_GREEN, LED_BLUE}) {
        digitalWrite(pin, HIGH);
        delay(150);
        digitalWrite(pin, LOW);
    }
}

void capture_sensor_data() {
    if (millis() - last_sample_time < SAMPLE_RATE_MS) return;
    last_sample_time = millis();

    if (sample_count >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXES_PER_SAMPLE) return;

    sensors_event_t a1, g1, t1;
    sensors_event_t a2, g2, t2;
    mpu_handle.getEvent(&a1, &g1, &t1);
    mpu_sheath.getEvent(&a2, &g2, &t2);

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

void run_inference() {
    int expected_samples = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXES_PER_SAMPLE;
    if (sample_count < expected_samples) {
        Serial.print("Not enough data: got ");
        Serial.print(sample_count);
        Serial.print(" / ");
        Serial.println(expected_samples);
        return;
    }

    signal_t features_signal;
    features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    features_signal.get_data = &raw_feature_get_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.print("Classifier error: ");
        Serial.println(res);
        return;
    }

    // Find highest-confidence prediction
    float max_val = 0;
    int max_idx = -1;
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > max_val) {
            max_val = result.classification[i].value;
            max_idx = i;
        }
    }

    if (max_idx == -1) return;

    const char* predicted_label = ei_classifier_inferencing_categories[max_idx];
    Serial.print("\n>>> ");
    Serial.print(predicted_label);
    Serial.print(" (");
    Serial.print(max_val * 100, 1);
    Serial.println("%)");

    // Match to spell and trigger LED
    for (int i = 0; i < NUM_SPELLS; i++) {
        if (strcmp(predicted_label, SPELLS[i].label) == 0) {
            Serial.print("Spell: ");
            Serial.println(SPELLS[i].name);
            Serial.println(SPELLS[i].effect);
            flash_led(SPELLS[i].led_pin);
            break;
        }
    }

    Serial.println();
}

bool read_button() {
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != last_button_state) {
        last_debounce_time = millis();
    }
    last_button_state = reading;
    return (millis() - last_debounce_time > DEBOUNCE_DELAY) && reading == HIGH;
}

void loop() {
    if (!capturing && read_button()) {
        Serial.println("Button pressed — starting capture...");
        sample_count = 0;
        capturing = true;
        capture_start_time = millis();
        last_sample_time = millis();
        all_leds_off();
        // Brief white flash to signal capture start
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_BLUE, HIGH);
        delay(100);
        all_leds_off();
    }

    if (capturing) {
        capture_sensor_data();
    }
}
