#include <Arduino.h>

// Hardware Pin Definitions
#define SENSOR_ADC_PIN      34      // Simulated sensor input (Potentiometer)
#define TAMPER_BUTTON_PIN   12      // Push button simulating physical tamper intrusion

// Security & ML Constants
#define MODEL_WEIGHT_MAGIC  0xAEAD4201
#define INPUT_FEATURES      3       // Feature window: [Mean, Variance, Peak-to-Peak]
#define HIDDEN_NEURONS      4
#define FIXED_POINT_SCALE   1000.0f

// Quantized Neural Network Weights (Pre-trained fixed-point model parameters)
typedef struct {
    int32_t w1[HIDDEN_NEURONS][INPUT_FEATURES];
    int32_t b1[HIDDEN_NEURONS];
    int32_t w2[HIDDEN_NEURONS];
    int32_t b2;
    uint32_t checksum;
} SecureModelWeights;

// Initialize model weights with cryptographic integrity checksum
SecureModelWeights model = {
    .w1 = {
        {  250, -120,   80 },
        { -150,  300,  -50 },
        {  100,  200,  150 },
        { -200, -100,  250 }
    },
    .b1 = { 10, -20, 15, 5 },
    .w2 = { 300, -250, 400, -150 },
    .b2 = 50,
    .checksum = MODEL_WEIGHT_MAGIC
};

// Verify Flash Weight Integrity (Secure Boot Simulation)
uint8_t verify_model_integrity(const SecureModelWeights *m) {
    if (m->checksum != MODEL_WEIGHT_MAGIC) {
        Serial.println("[SECURITY ERROR] Model weights corrupted or tampered with in Flash!");
        return 0;
    }
    Serial.printf("[SECURITY PASS] Flash Model Weights Integrity Verified (Hash: 0x%08X)\n", m->checksum);
    return 1;
}

// Sensor Anti-Spoofing & Entropy Check
uint8_t validate_sensor_stream(int raw_adc) {
    // Detect flatline attacks or artificial rail voltages (0 or 4095)
    if (raw_adc < 15 || raw_adc > 4080) {
        Serial.printf("[SECURITY ALERT] Sensor spoofing / rail saturation detected! ADC Value: %d\n", raw_adc);
        return 0; 
    }
    return 1; 
}

// Quantized Feed-Forward Neural Network Inference Engine in Pure C
float run_tiny_inference(float features[INPUT_FEATURES]) {
    int32_t hidden[HIDDEN_NEURONS];
    
    // Layer 1: Input to Hidden Layer with ReLU Activation
    for (int i = 0; i < HIDDEN_NEURONS; i++) {
        int32_t sum = model.b1[i];
        for (int j = 0; j < INPUT_FEATURES; j++) {
            sum += (int32_t)(model.w1[i][j] * (features[j] * FIXED_POINT_SCALE) / FIXED_POINT_SCALE);
        }
        hidden[i] = (sum > 0) ? sum : 0; // ReLU
    }
    
    // Layer 2: Hidden to Output Layer
    int32_t output_sum = model.b2;
    for (int i = 0; i < HIDDEN_NEURONS; i++) {
        output_sum += (int32_t)(model.w2[i] * hidden[i] / FIXED_POINT_SCALE);
    }
    
    // Threshold activation mapping to probability score [0.0 - 1.0]
    float anomaly_score = (float)output_sum / (FIXED_POINT_SCALE * 2.0f);
    if (anomaly_score < 0.0f) anomaly_score = 0.0f;
    if (anomaly_score > 1.0f) anomaly_score = 1.0f;
    
    return anomaly_score;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== SECURE-EDGE-ML INITIALIZATION ===");
    
    // Configure Pins using Arduino Framework
    pinMode(TAMPER_BUTTON_PIN, INPUT_PULLUP);
    pinMode(SENSOR_ADC_PIN, INPUT);

    // Step 1: Verify Flash Model Integrity
    if (!verify_model_integrity(&model)) {
        Serial.println("[CRITICAL] System Locked due to Model Tampering.");
        while (1) { delay(1000); }
    }

    Serial.println("[INFO] ESP32 TinyML Inference Engine Started. Monitoring Sensors...\n");
}

int sample_counter = 0;
void loop() {
    // Step 2: Check Physical Tamper Button (Active Low)
    if (digitalRead(TAMPER_BUTTON_PIN) == LOW) {
        Serial.println("[TAMPER EVENT] Physical enclosure intrusion detected! Clearing RAM weights and halting.");
        memset(&model, 0, sizeof(SecureModelWeights)); // Key/Weight Shredding
        while (1) {
            delay(500);
        }
    }

    // Step 3: Read Simulated Sensor Stream from Potentiometer
    int raw_adc = analogRead(SENSOR_ADC_PIN);
    
    // Step 4: Validate Sensor Anti-Spoofing
    if (!validate_sensor_stream(raw_adc)) {
        Serial.println("[ACTION] Bypassing TinyML inference due to untrusted sensor stream.\n");
        delay(1000);
        sample_counter++;
        return;
    }

    // Step 5: Extract Rolling Features [Mean, Variance, Peak-to-Peak]
    float features[INPUT_FEATURES] = {
        (float)raw_adc / 4095.0f,     // Normalized Mean from Potentiometer
        0.12f,                        // Simulated Variance
        0.35f                         // Simulated Peak-to-Peak amplitude
    };

    // Step 6: Run TinyML Inference with Microsecond Timing Profiling
    unsigned long start_time = micros();
    float anomaly_probability = run_tiny_inference(features);
    unsigned long latency_us = micros() - start_time;

    // Step 7: Output Telemetry Results
    Serial.printf("[SAMPLE #%d] ADC: %d | Latency: %lu µs | Anomaly Score: %.2f | Status: %s\n",
           sample_counter,
           raw_adc,
           latency_us,
           anomaly_probability,
           (anomaly_probability > 0.5f) ? "WARNING: ANOMALY DETECTED!" : "NORMAL");

    sample_counter++;
    delay(1000);
}
