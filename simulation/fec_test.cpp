#include <iostream>
#include <iomanip>
#include "../src/LithoMeshFEC.h"

using namespace std;

#define DATA_BLOCKS 5
#define BLOCK_SIZE 120

int main() {
    cout << "================================================\n";
    cout << " LITHOMESH FEC C++ LAYER TEST (XOR BLOCK ERASURE)\n";
    cout << "================================================\n\n";

    uint8_t data[DATA_BLOCKS][BLOCK_SIZE];
    uint8_t parity[BLOCK_SIZE];

    // 1. Fill data with some patterns
    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        for (size_t j = 0; j < BLOCK_SIZE; j++) {
            data[i][j] = (uint8_t)(i * 10 + j);
        }
    }

    // 2. Generate parity
    LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::generate_parity(data, parity);

    // 3. Simulate packet drop
    size_t dropped_block = 2;
    cout << "[*] Simulating packet drop on block index " << dropped_block << "...\n";
    uint8_t original_first_byte = data[dropped_block][0];
    
    // Wipe the block
    memset(data[dropped_block], 0, BLOCK_SIZE);
    
    // 4. Recover
    cout << "[*] Recovering dropped block using parity...\n";
    bool success = LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(data, parity, dropped_block);
    
    if (success && data[dropped_block][0] == original_first_byte) {
        cout << "[+] SUCCESS: Data perfectly recovered!\n";
    } else {
        cout << "[-] FAIL: Recovery failed.\n";
    }

    return 0;
}
