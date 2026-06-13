#include <Arduino.h>
#include "LithoMesh.h"
#include "LithoMeshFEC.h"

// ==============================================================
// CONFIGURATION
// Flash one ESP32 with ROLE_NODE_A true, the other with false
// Connect: GND -> GND, TX2 (17) -> RX2 (16), RX2 (16) -> TX2 (17)
// ==============================================================
#define ROLE_NODE_A true

#define RADIO_SERIAL Serial2
#define RADIO_RX_PIN 16
#define RADIO_TX_PIN 17

#define DATA_BLOCKS 20
#define BLOCK_SIZE 120
#define PACKET_SIZE 121 // 1 byte Seq_ID + 120 bytes payload

LithoMeshEngine<200, 3, 2000, 4> localNode;

// Static buffers to avoid Stack Overflow
static uint8_t raw_payload[2400];
static uint8_t data_blocks[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t parity_block[BLOCK_SIZE];
static uint8_t rx_buffer[PACKET_SIZE];
static bool packet_received[DATA_BLOCKS + 1];
static uint8_t recovered_data[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t received_parity[BLOCK_SIZE];
static uint8_t reconstructed_payload[2400];
static uint32_t added_buf[200], removed_buf[200];

// Test Simulation Overrides
int burst_loss_injection = 0;

void setup() {
    Serial.begin(115200);
    // Expand hardware serial buffer to prevent UART overflow drops
    RADIO_SERIAL.setRxBufferSize(4096); 
    RADIO_SERIAL.begin(115200, SERIAL_8N1, RADIO_RX_PIN, RADIO_TX_PIN);

    while (!Serial); delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 6: Dual ESP32 UART ");
    Serial.print(" Role: "); Serial.println(ROLE_NODE_A ? "NODE A (Sender)" : "NODE B (Receiver)");
    Serial.println("============================================");

    for(uint32_t i=1; i<=1000; i++) localNode.log_event(i);

    if (ROLE_NODE_A) {
        for(uint32_t i=1001; i<=1050; i++) localNode.log_event(i);
        Serial.println("[STATE] Divergence Applied: 50 local events.");
        Serial.println("Commands: SYNC, LOSS 1, LOSS 2");
    } else {
        Serial.println("[STATE] Listening for Sync...");
    }
}

void tx_framed_iblt() {
    uint32_t start_heap = ESP.getFreeHeap();
    uint32_t t_tx_start = millis();
    
    // Chunk payload
    memcpy(raw_payload, &localNode.iblt.cells, sizeof(localNode.iblt.cells));
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(data_blocks[i], raw_payload + (i * BLOCK_SIZE), BLOCK_SIZE);
    }
    
    // Generate Parity
    LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::generate_parity(data_blocks, parity_block);

    // TX over Serial2
    int drop_start = burst_loss_injection > 0 ? random(DATA_BLOCKS - burst_loss_injection) : -1;
    
    RADIO_SERIAL.write('S'); RADIO_SERIAL.write('Y'); // Magic Header
    
    for(int i=0; i<=DATA_BLOCKS; i++) {
        if (burst_loss_injection > 0 && i >= drop_start && i < drop_start + burst_loss_injection) {
            // Drop entire frame
            continue;
        }
        
        RADIO_SERIAL.write((uint8_t)i); // Seq_ID
        if (i < DATA_BLOCKS) {
            RADIO_SERIAL.write(data_blocks[i], BLOCK_SIZE);
        } else {
            RADIO_SERIAL.write(parity_block, BLOCK_SIZE);
        }
    }
    Serial.printf("[TX] Sent %d frames in %d ms\n", DATA_BLOCKS + 1 - burst_loss_injection, millis() - t_tx_start);
}

void rx_framed_iblt() {
    uint32_t t_rx_start = millis();
    
    memset(packet_received, 0, sizeof(packet_received));
    int packets_got = 0;
    
    // Non-blocking timeout based RX loop
    while(millis() - t_rx_start < 2000) {
        if (RADIO_SERIAL.available() >= PACKET_SIZE) {
            RADIO_SERIAL.readBytes(rx_buffer, PACKET_SIZE);
            uint8_t seq = rx_buffer[0];
            if (seq <= DATA_BLOCKS) {
                packet_received[seq] = true;
                if (seq < DATA_BLOCKS) {
                    memcpy(recovered_data[seq], rx_buffer + 1, BLOCK_SIZE);
                } else {
                    memcpy(received_parity, rx_buffer + 1, BLOCK_SIZE);
                }
                packets_got++;
            }
        }
        
        // Break early if we got all packets
        if (packets_got >= DATA_BLOCKS + 1) break; 
        
        // Break early if line went quiet (to handle dropped packets fast)
        if (packets_got > 0 && RADIO_SERIAL.available() == 0 && (millis() - t_rx_start > 500)) break;
    }
    
    Serial.printf("[RX] Received %d/%d frames.\n", packets_got, DATA_BLOCKS + 1);
    
    // FEC Recovery
    int missing_count = 0;
    int first_missing_idx = -1;
    for(int i=0; i<=DATA_BLOCKS; i++) {
        if(!packet_received[i]) {
            missing_count++;
            if (first_missing_idx == -1) first_missing_idx = i;
        }
    }
    
    if (missing_count > 0) {
        if (missing_count == 1 && first_missing_idx != -1 && first_missing_idx != DATA_BLOCKS) {
            Serial.println("  -> [FEC] Engaging XOR recovery for 1 missing frame...");
            bool rec = LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(recovered_data, received_parity, first_missing_idx);
            if (!rec) { Serial.println("  -> [FEC] Recovery failed internally."); return; }
        } else {
            Serial.printf("  -> [FEC] Collapse! Lost %d frames. Aborting.\n", missing_count);
            return;
        }
    }
    
    // Reconstruct and Decode
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(reconstructed_payload + (i * BLOCK_SIZE), recovered_data[i], BLOCK_SIZE);
    }
    
    static IBLT<200, 3> incoming_iblt;
    memcpy(&incoming_iblt.cells, reconstructed_payload, sizeof(incoming_iblt.cells));

    static IBLT<200, 3> delta;
    incoming_iblt.subtract(localNode.iblt, delta);

    size_t added_count = 0, removed_count = 0, iterations = 0;
    uint32_t t_decode_start = millis();
    bool success = delta.decode(added_buf, 200, &added_count, removed_buf, 200, &removed_count, &iterations);
    
    Serial.println("\n--- METRICS ---");
    Serial.printf("Decode Result     : %s\n", success ? "PASS" : "FAIL");
    Serial.printf("Decode Iterations : %d\n", iterations);
    Serial.printf("Decode Time       : %d ms\n", millis() - t_decode_start);
    
    if (success && added_count > 0) {
        // Send back requests
        Serial.printf("[TX] Requesting %d missing IDs...\n", added_count);
        for(size_t i=0; i<added_count; i++) {
            RADIO_SERIAL.write('R'); RADIO_SERIAL.write('Q');
            RADIO_SERIAL.write((uint8_t*)&added_buf[i], 4);
        }
    }
}

void process_command(String cmd) {
    if (cmd == "SYNC") {
        burst_loss_injection = 0;
        Serial.println("\n[SYNC] Initiating...");
        tx_framed_iblt();
    } else if (cmd.startsWith("LOSS ")) {
        burst_loss_injection = cmd.substring(5).toInt();
        Serial.printf("[INJECT] BURSTLOSS set to drop %d contiguous frames on next SYNC.\n", burst_loss_injection);
    }
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        process_command(cmd);
    }
    
    // Node B listening for SYNC
    if (RADIO_SERIAL.available() >= 2) {
        char c1 = RADIO_SERIAL.peek();
        if (c1 == 'S') {
            RADIO_SERIAL.read(); // consume S
            char c2 = RADIO_SERIAL.read();
            if (c2 == 'Y') {
                Serial.println("\n[RX] Sync Header Detected. Catching frames...");
                rx_framed_iblt();
            }
        } else if (c1 == 'R') {
            RADIO_SERIAL.read();
            char c2 = RADIO_SERIAL.read();
            if (c2 == 'Q') {
                uint32_t missing_id;
                RADIO_SERIAL.readBytes((uint8_t*)&missing_id, 4);
                Serial.printf("[RX] Peer requested ID: %d\n", missing_id);
            }
        } else {
            RADIO_SERIAL.read(); // throw away garbage
        }
    }
}
