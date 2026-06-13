#include <iostream>
#include "LithoMesh.h"

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  LithoMesh Embedded C++ Port Validation  " << std::endl;
    std::cout << "==========================================" << std::endl;

    // We instantiate an engine with:
    // - 100 IBLT Cells (100 * 12 bytes = 1200 bytes)
    // - 3 Hash functions for IBLT
    // - 1000 bits for Bloom Filter (125 bytes)
    // - 4 Hash functions for Bloom Filter
    // Total memory footprint per node should be ~1325 bytes
    LithoMeshEngine<100, 3, 1000, 4> nodeA;
    LithoMeshEngine<100, 3, 1000, 4> nodeB;

    std::cout << "[MEMORY] LithoMeshEngine Size: " << sizeof(nodeA) << " bytes" << std::endl;

    // Both nodes see events 1 through 50
    for(uint32_t i = 1; i <= 50; i++) {
        nodeA.log_event(i);
        nodeB.log_event(i);
    }

    // Node A receives new events while Node B is offline
    nodeA.log_event(1001);
    nodeA.log_event(1002);
    
    // Test the Idempotency Guard: Node A receives event 1001 again
    bool dup_inserted = nodeA.log_event(1001);
    std::cout << "[LOGIC]  Duplicate insertion allowed? " << (dup_inserted ? "YES" : "NO") << std::endl;

    // Reconnection! Perform Sublinear Reconciliation
    IBLT<100, 3> delta;
    nodeA.iblt.subtract(nodeB.iblt, delta);

    // Pre-allocate recovery arrays (static memory, no malloc)
    uint32_t added[10], removed[10];
    size_t added_count = 0, removed_count = 0;

    bool success = delta.decode(added, 10, &added_count, removed, 10, &removed_count);
    
    std::cout << "[SYNC]   Decode Success: " << (success ? "YES" : "NO") << std::endl;
    std::cout << "[SYNC]   Recovered Missing Events: " << added_count << std::endl;
    for(size_t i = 0; i < added_count; i++) {
        std::cout << "         -> Event ID: " << added[i] << std::endl;
    }

    return 0;
}
