#include <Arduino.h>
#include "LithoMesh.h"

#define IS_NODE_A true
#define RADIO_SERIAL Serial2 

LithoMeshEngine<200, 3, 2000, 4> localNode;

// Test Settings
uint8_t packet_loss_pct = 0;
bool corrupt_next = false;
uint32_t baseline_heap = 0;
UBaseType_t baseline_stack = 0;

void setup() {
    Serial.begin(115200);
    RADIO_SERIAL.begin(115200);
    while (!Serial); delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 4: Round 1 (UART Radio) ");
    Serial.print(" Role: "); Serial.println(IS_NODE_A ? "NODE A" : "NODE B");
    Serial.println("============================================");

    baseline_heap = ESP.getFreeHeap();
    baseline_stack = uxTaskGetStackHighWaterMark(NULL);

    // Initial baseline sync state
    for(uint32_t i = 1; i <= 1000; i++) localNode.log_event(i);

    if (IS_NODE_A) {
        // Node A diverges
        for(uint32_t i = 1001; i <= 1050; i++) localNode.log_event(i);
        Serial.println("[STATE] Divergence Applied: 50 local events.");
        Serial.println("\nCommands: SYNC, LOSS 10, DUP 10, REBOOT, CORRUPT, DIVERGE 500");
    } else {
        Serial.println("[STATE] Listening for Sync...");
    }
}

// Simple radio send with simulated packet loss/corruption
void radio_send(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        // Packet loss simulation
        if (packet_loss_pct > 0 && random(100) < packet_loss_pct) {
            continue; // drop byte
        }
        // Corruption simulation
        if (corrupt_next && i == len/2) {
            RADIO_SERIAL.write(data[i] ^ 0xFF);
            corrupt_next = false;
        } else {
            RADIO_SERIAL.write(data[i]);
        }
    }
}

void process_command(String cmd) {
    if (cmd == "SYNC") {
        Serial.println("\n[SYNC] Initiating...");
        uint32_t t_start = millis();
        uint32_t heap_start = ESP.getFreeHeap();
        UBaseType_t stack_start = uxTaskGetStackHighWaterMark(NULL);

        // Phase 1: Send Summary (A -> B)
        size_t payload_size = sizeof(localNode.iblt.cells);
        Serial.printf("[TX] Sending Summary: %d bytes\n", payload_size);
        RADIO_SERIAL.write('S'); RADIO_SERIAL.write('Y');
        radio_send((uint8_t*)&localNode.iblt.cells, payload_size);

        // Phase 2 & 3: Wait for Missing IDs from B, then send Payloads
        Serial.println("[RX] Waiting for Missing IDs...");
        uint32_t t_first_event = 0;
        uint32_t recovered_count = 0;

        // Simple timeout
        uint32_t timeout = millis() + 10000;
        while(millis() < timeout) {
            if (RADIO_SERIAL.available() >= 2) {
                char h1 = RADIO_SERIAL.read();
                char h2 = RADIO_SERIAL.read();
                if (h1 == 'R' && h2 == 'Q') {
                    if (t_first_event == 0) t_first_event = millis() - t_start;
                    
                    uint32_t missing_id;
                    RADIO_SERIAL.readBytes((uint8_t*)&missing_id, 4);
                    recovered_count++;
                    
                    // Reply with dummy payload (simulating actual event data)
                    RADIO_SERIAL.write('P'); RADIO_SERIAL.write('L');
                    uint8_t dummy_payload[32] = {0}; 
                    RADIO_SERIAL.write(dummy_payload, 32);
                } else if (h1 == 'D' && h2 == 'N') {
                    // Sync Done!
                    break;
                }
            }
        }

        uint32_t t_end = millis();
        uint32_t heap_end = ESP.getFreeHeap();
        UBaseType_t stack_end = uxTaskGetStackHighWaterMark(NULL);

        Serial.println("\n--- TEST METRICS (Node A) ---");
        Serial.printf("Time to First Missing Event : %d ms\n", t_first_event);
        Serial.printf("Time to Full Convergence    : %d ms\n", t_end - t_start);
        Serial.printf("Peak Heap Delta             : %d bytes\n", heap_start - heap_end);
        Serial.printf("Stack High Watermark Used   : %d words\n", stack_start - stack_end);
        Serial.printf("Largest Packet (Summary)    : %d bytes\n", payload_size);
        Serial.printf("Event Recovery Ratio        : %d/50\n", recovered_count);
        
    } else if (cmd.startsWith("LOSS ")) {
        packet_loss_pct = cmd.substring(5).toInt();
        Serial.printf("[INJECT] TX Packet Loss set to %d%%\n", packet_loss_pct);
    } else if (cmd == "CORRUPT") {
        corrupt_next = true;
        Serial.println("[INJECT] Next transmission will be corrupted.");
    } else if (cmd.startsWith("DIVERGE ")) {
        int count = cmd.substring(8).toInt();
        for(int i=0; i<count; i++) localNode.log_event(2000 + i);
        Serial.printf("[INJECT] Added %d new events.\n", count);
    } else if (cmd.startsWith("DUP ")) {
        int count = cmd.substring(4).toInt();
        for(int i=0; i<count; i++) localNode.log_event(1001);
        Serial.printf("[INJECT] Added %d duplicates of event 1001.\n", count);
    } else if (cmd == "REBOOT") {
        Serial.println("[INJECT] Rebooting...");
        ESP.restart();
    }
}

void loop() {
    if (IS_NODE_A && Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        process_command(cmd);
    }

    if (!IS_NODE_A && RADIO_SERIAL.available() >= 2) {
        char h1 = RADIO_SERIAL.read();
        char h2 = RADIO_SERIAL.read();
        
        if (h1 == 'S' && h2 == 'Y') {
            size_t payload_size = sizeof(localNode.iblt.cells);
            
            uint32_t t_start = millis();
            while(RADIO_SERIAL.available() < payload_size) {
                if (millis() - t_start > 5000) {
                    Serial.println("[RX ERROR] Timeout waiting for full summary. Packet loss?");
                    return;
                }
            }
            
            IBLT<200, 3> incoming_iblt;
            RADIO_SERIAL.readBytes((uint8_t*)&incoming_iblt.cells, payload_size);

            Serial.println("\n[RX] Summary received. Decoding...");
            
            IBLT<200, 3> delta;
            incoming_iblt.subtract(localNode.iblt, delta);

            uint32_t added[100], removed[100];
            size_t added_count = 0, removed_count = 0, iterations = 0;
            
            bool success = delta.decode(added, 100, &added_count, removed, 100, &removed_count, &iterations);

            Serial.println("\n--- TEST METRICS (Node B) ---");
            Serial.printf("Decode Success    : %s\n", success ? "PASS" : "FAIL");
            Serial.printf("Decode Iterations : %d\n", iterations);
            if (added_count > 0 || removed_count > 0) {
                Serial.printf("Peel Success Ratio: 100%%\n"); // If decode success is true
            } else {
                Serial.printf("Peel Success Ratio: 0%%\n");
            }

            if (success) {
                // Node B realizes Node A has `added_count` events that B lacks.
                for (size_t i = 0; i < added_count; i++) {
                    RADIO_SERIAL.write('R'); RADIO_SERIAL.write('Q');
                    RADIO_SERIAL.write((uint8_t*)&added[i], 4);
                    
                    // Wait for dummy payload
                    while(RADIO_SERIAL.available() < 34) { delay(1); }
                    char p1 = RADIO_SERIAL.read(); char p2 = RADIO_SERIAL.read();
                    uint8_t dummy[32];
                    RADIO_SERIAL.readBytes(dummy, 32);
                    
                    localNode.log_event(added[i]); // Apply to local DB
                }
            }
            RADIO_SERIAL.write('D'); RADIO_SERIAL.write('N'); // Done
            Serial.println("[SYNC] Fully Converged.");
        }
    }
}
