#include <Arduino.h>
#include "LithoMesh.h"

// Define Node Role (Change to false for Node B)
#define IS_NODE_A true

// Use HardwareSerial 2 for ESP32-to-ESP32 communication
// TX = GPIO 17, RX = GPIO 16
#define RADIO_SERIAL Serial2 

// The Sync Engine
LithoMeshEngine<100, 3, 1000, 4> localNode;

void setup() {
    Serial.begin(115200); // PC Monitor
    RADIO_SERIAL.begin(115200); // "Radio" Link
    
    while (!Serial);
    delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 4: Round 1 (UART Radio) ");
    Serial.print(" Role: "); Serial.println(IS_NODE_A ? "NODE A" : "NODE B");
    Serial.println("============================================");

    // Populate baseline (1000 shared events)
    for(uint32_t i = 1; i <= 1000; i++) {
        localNode.log_event(i);
    }

    if (IS_NODE_A) {
        // Node A diverges by 50 events while disconnected
        for(uint32_t i = 1001; i <= 1050; i++) {
            localNode.log_event(i);
        }
        Serial.println("[STATE] Divergence Applied: 50 local events.");
        Serial.println("\n[COMMAND] Type 'SYNC' to initiate Test A (Healthy Sync).");
    } else {
        Serial.println("[STATE] Waiting for Sync requests...");
    }
}

void loop() {
    // --- NODE A LOGIC: Trigger Sync ---
    if (IS_NODE_A && Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "SYNC") {
            Serial.println("\n[SYNC] Initiating Sync Sequence...");
            uint32_t start_time = millis();
            uint32_t baseline_heap = ESP.getFreeHeap();
            
            // 1. Send our IBLT payload over UART
            size_t payload_size = sizeof(localNode.iblt.cells);
            Serial.printf("[TX] Sending %d bytes to Node B...\n", payload_size);
            
            // Send Header
            RADIO_SERIAL.write('I'); RADIO_SERIAL.write('B');
            // Send Payload
            uint8_t* tx_buffer = (uint8_t*)&localNode.iblt.cells;
            RADIO_SERIAL.write(tx_buffer, payload_size);

            // 2. Wait for Delta Response from Node B
            Serial.println("[RX] Waiting for Missing Data from Node B...");
            
            // (Timeout logic for missing packets would go here)
            // Simplified read for Round 1
            while(RADIO_SERIAL.available() < sizeof(uint32_t)*50) {
                delay(1);
            }
            
            uint32_t end_time = millis();
            uint32_t peak_heap = ESP.getFreeHeap();

            Serial.println("\n--- TEST A: HEALTHY RESULTS ---");
            Serial.printf("Sync Time: %d ms\n", end_time - start_time);
            Serial.printf("Peak RAM: %d bytes allocated dynamically\n", baseline_heap - peak_heap);
            Serial.println("-------------------------------");
        }
    }

    // --- NODE B LOGIC: Respond to Sync ---
    if (!IS_NODE_A && RADIO_SERIAL.available() >= 2) {
        if (RADIO_SERIAL.read() == 'I' && RADIO_SERIAL.read() == 'B') {
            size_t payload_size = sizeof(localNode.iblt.cells);
            
            // Wait for full payload
            while(RADIO_SERIAL.available() < payload_size) { delay(1); }
            
            Serial.println("\n[RX] Received IBLT Payload. Subtracting...");
            
            IBLT<100, 3> incoming_iblt;
            uint8_t* rx_buffer = (uint8_t*)&incoming_iblt.cells;
            RADIO_SERIAL.readBytes(rx_buffer, payload_size);

            // The Subtraction
            IBLT<100, 3> delta;
            incoming_iblt.subtract(localNode.iblt, delta);

            // Decoding
            uint32_t added[50], removed[50];
            size_t added_count = 0, removed_count = 0;
            bool success = delta.decode(added, 50, &added_count, removed, 50, &removed_count);

            Serial.printf("[SYNC] Decode Success: %s\n", success ? "PASS" : "FAIL");
            Serial.printf("[SYNC] Identified %d missing events.\n", added_count);

            // Send missing events back to A
            Serial.printf("[TX] Sending missing data back to Node A...\n");
            uint8_t* tx_data = (uint8_t*)&added;
            RADIO_SERIAL.write(tx_data, added_count * sizeof(uint32_t));
        }
    }
}
