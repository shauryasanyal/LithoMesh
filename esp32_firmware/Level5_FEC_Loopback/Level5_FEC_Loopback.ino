#include <Arduino.h>
#include "LithoMesh.h"
#include "LithoMeshFEC.h"

#define DATA_BLOCKS 20
#define BLOCK_SIZE 120

LithoMeshEngine<200, 3, 2000, 4> nodeA;
LithoMeshEngine<200, 3, 2000, 4> nodeB;

struct LoraPacket {
    uint8_t seq_id;
    uint8_t payload[BLOCK_SIZE];
};

uint32_t baseline_heap = 0;
UBaseType_t baseline_stack = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 5: FEC Loopback Test ");
    Serial.println(" Hardware: Single ESP32 (No Radio) ");
    Serial.println("============================================");

    baseline_heap = ESP.getFreeHeap();
    baseline_stack = uxTaskGetStackHighWaterMark(NULL);

    Serial.println("\nCommands: ");
    Serial.println("  SOAK 100    - Run 100 healthy sync cycles");
    Serial.println("  BURSTLOSS 1 - Drop 1 frame per cycle (Recoverable)");
    Serial.println("  BURSTLOSS 2 - Drop 2 frames per cycle (Collapse boundary)");
    Serial.println("  BURSTLOSS 3 - Drop 3 frames per cycle");
    Serial.println("  REBOOT      - Restart ESP32");
}

void reset_nodes() {
    nodeA = LithoMeshEngine<200, 3, 2000, 4>();
    nodeB = LithoMeshEngine<200, 3, 2000, 4>();
    for(uint32_t i=1; i<=1000; i++) {
        nodeA.log_event(i);
        nodeB.log_event(i);
    }
}

void diverge(int amount) {
    for(int i=0; i<amount; i++) {
        nodeA.log_event(2000 + i);
    }
}

bool run_loopback_cycle(int burst_loss, bool print_metrics) {
    uint32_t t_start = millis();
    uint32_t heap_start = ESP.getFreeHeap();
    
    // --- 1. GENERATE ---
    uint8_t raw_payload[2400];
    memcpy(raw_payload, &nodeA.iblt.cells, sizeof(nodeA.iblt.cells));
    
    uint8_t data[DATA_BLOCKS][BLOCK_SIZE];
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(data[i], raw_payload + (i * BLOCK_SIZE), BLOCK_SIZE);
    }
    
    // --- 2. ENCODE (FEC Parity) ---
    uint8_t parity[BLOCK_SIZE];
    LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::generate_parity(data, parity);

    // --- 3. DROP PACKET (Simulated Air) ---
    LoraPacket received_packets[DATA_BLOCKS + 1];
    bool packet_received[DATA_BLOCKS + 1];
    for(int i=0; i<=DATA_BLOCKS; i++) packet_received[i] = true;
    
    // Inject burst loss (sequential packets dropped)
    if (burst_loss > 0) {
        int drop_start = random(DATA_BLOCKS - burst_loss);
        for(int i=0; i<burst_loss; i++) {
            packet_received[drop_start + i] = false;
        }
    }
    
    for(int i=0; i<=DATA_BLOCKS; i++) {
        if(!packet_received[i]) continue;
        received_packets[i].seq_id = i;
        if (i < DATA_BLOCKS) memcpy(received_packets[i].payload, data[i], BLOCK_SIZE);
        else memcpy(received_packets[i].payload, parity, BLOCK_SIZE);
    }
    
    // --- 4. RECOVER (Node B) ---
    uint8_t recovered_data[DATA_BLOCKS][BLOCK_SIZE];
    uint8_t received_parity[BLOCK_SIZE];
    int missing_count = 0;
    int first_missing_idx = -1;
    
    for(int i=0; i<DATA_BLOCKS; i++) {
        if(packet_received[i]) {
            memcpy(recovered_data[i], received_packets[i].payload, BLOCK_SIZE);
        } else {
            missing_count++;
            if (first_missing_idx == -1) first_missing_idx = i;
        }
    }
    
    if (packet_received[DATA_BLOCKS]) {
        memcpy(received_parity, received_packets[DATA_BLOCKS].payload, BLOCK_SIZE);
    } else {
        missing_count++; // Parity packet lost
    }
    
    bool fec_success = true;
    if (missing_count > 0) {
        if (missing_count == 1 && first_missing_idx != -1) {
            fec_success = LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(recovered_data, received_parity, first_missing_idx);
            if (print_metrics) Serial.println("   [FEC] Recovered 1 missing frame successfully.");
        } else {
            if (print_metrics) Serial.printf("   [FEC] Collapse: %d frames lost. Recovery impossible.\n", missing_count);
            fec_success = false;
        }
    }
    
    if (!fec_success) return false;

    // --- 5. DECODE ---
    uint8_t reconstructed_payload[2400];
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(reconstructed_payload + (i * BLOCK_SIZE), recovered_data[i], BLOCK_SIZE);
    }

    IBLT<200, 3> incoming_iblt;
    memcpy(&incoming_iblt.cells, reconstructed_payload, sizeof(incoming_iblt.cells));

    IBLT<200, 3> delta;
    incoming_iblt.subtract(nodeB.iblt, delta);

    uint32_t added[200], removed[200];
    size_t added_count = 0, removed_count = 0, iterations = 0;
    
    bool success = delta.decode(added, 200, &added_count, removed, 200, &removed_count, &iterations);
    
    if (print_metrics) {
        uint32_t t_end = millis();
        Serial.printf("   Decode Result     : %s\n", success ? "PASS" : "FAIL");
        Serial.printf("   Decode Iterations : %d\n", iterations);
        Serial.printf("   Time to Decode    : %d ms\n", t_end - t_start);
    }
    
    return success;
}

void execute_soak(int cycles, int burst_loss) {
    Serial.printf("\n--- STARTING SOAK TEST (Cycles: %d, Burst Loss: %d) ---\n", cycles, burst_loss);
    int failures = 0;
    
    uint32_t t_start = millis();
    for (int i=0; i<cycles; i++) {
        reset_nodes(); 
        diverge(50);
        bool res = run_loopback_cycle(burst_loss, false);
        if (!res) failures++;
        
        if (i > 0 && i % 20 == 0) {
            Serial.printf("   Cycle %d... \n", i);
        }
    }
    uint32_t t_end = millis();
    
    Serial.println("\n--- SOAK TEST RESULTS ---");
    Serial.printf("Cycles Completed : %d\n", cycles);
    Serial.printf("Failures         : %d\n", failures);
    Serial.printf("Total Soak Time  : %d ms\n", t_end - t_start);
    Serial.printf("Peak Heap Delta  : %d bytes\n", baseline_heap - ESP.getFreeHeap());
}

void process_command(String cmd) {
    if (cmd.startsWith("SOAK ")) {
        int cycles = cmd.substring(5).toInt();
        if (cycles == 0) cycles = 100;
        execute_soak(cycles, 0);
    } else if (cmd.startsWith("BURSTLOSS ")) {
        int loss = cmd.substring(10).toInt();
        Serial.printf("\n[TEST] Running BURSTLOSS %d...\n", loss);
        reset_nodes();
        diverge(50);
        run_loopback_cycle(loss, true);
    } else if (cmd == "REBOOT") {
        Serial.println("Rebooting...");
        ESP.restart();
    } else {
        Serial.println("Unknown command.");
    }
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        process_command(cmd);
    }
}
