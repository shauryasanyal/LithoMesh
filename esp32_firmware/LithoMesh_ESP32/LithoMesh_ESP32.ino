#include "LithoMesh.h"
#include <Arduino.h>

// Simulated Network Link (BLE/LoRa payload size limitation)
#define PACKET_SIZE 250 // Typical LoRa/BLE MTU

LithoMeshEngine<100, 3, 1000, 4> nodeA;
LithoMeshEngine<100, 3, 1000, 4> nodeB;

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n\n============================================");
    Serial.println("  LithoMesh v0.3 - Level 4 Hardware Test  ");
    Serial.println("============================================");

    // 1. Memory Profiling - Baseline
    uint32_t baseline_heap = ESP.getFreeHeap();
    UBaseType_t baseline_stack = uxTaskGetStackHighWaterMark(NULL);
    
    Serial.printf("[MEM] Initial Free Heap: %d bytes\n", baseline_heap);

    // 2. Generate 1000 events on both nodes (Synced State)
    for(uint32_t i = 1; i <= 1000; i++) {
        nodeA.log_event(i);
        nodeB.log_event(i);
    }

    // 3. DISCONNECT - Node A gets 50 new events offline
    for(uint32_t i = 1001; i <= 1050; i++) {
        nodeA.log_event(i);
    }

    // 4. RECONNECT & MEASURE
    Serial.println("\n--- Starting Sync Phase ---");
    
    // Track CPU Time
    uint32_t start_time = micros();

    // The Subtraction (Engine Logic)
    IBLT<100, 3> delta;
    nodeA.iblt.subtract(nodeB.iblt, delta);

    // Decoding
    uint32_t added[50], removed[50];
    size_t added_count = 0, removed_count = 0;
    bool success = delta.decode(added, 50, &added_count, removed, 50, &removed_count);

    uint32_t end_time = micros();

    // 5. Memory Profiling - Post-Sync Peak
    uint32_t peak_heap = ESP.getFreeHeap();
    UBaseType_t peak_stack = uxTaskGetStackHighWaterMark(NULL);
    
    // 6. Packet Serialization (Simulated MTU Chunking)
    size_t payload_size = sizeof(delta.cells);
    size_t packets_needed = (payload_size / PACKET_SIZE) + 1;

    Serial.println("\n[RESULTS]");
    Serial.printf("CPU Time (Sync & Decode) : %d microseconds\n", (end_time - start_time));
    Serial.printf("Peak Stack Used          : %d words\n", baseline_stack - peak_stack);
    Serial.printf("Heap Memory Used         : %d bytes (Dynamic Allocation)\n", baseline_heap - peak_heap);
    
    Serial.printf("Total Payload Transmitted: %d bytes\n", payload_size);
    Serial.printf("MTU Packets Required     : %d packets (assuming 250B MTU)\n", packets_needed);
    Serial.printf("Decode Status            : %s\n", success ? "PASS" : "FAIL");
    Serial.printf("Missing Events Recovered : %d\n", added_count);

    Serial.println("\n[LITHOMESH ENVELOPE]");
    Serial.println("Divergence: <=10%");
    Serial.println("Operation : Sparse Delta Recovery");
}

void loop() {
    // Hardware test only runs once
    delay(10000);
}
