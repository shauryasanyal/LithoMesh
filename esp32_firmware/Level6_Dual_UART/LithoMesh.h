#ifndef LITHOMESH_H
#define LITHOMESH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// ==========================================
// Fast Hash Function (Wang Hash for embedded)
// ==========================================
// We drop heavy crypto (like SHA256) but need strong avalanche for sequential IDs.
inline uint32_t wang_hash(uint32_t key, uint32_t seed) {
    key = key ^ seed;
    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * 0x27d4eb2d;
    key = key ^ (key >> 15);
    return key;
}

    inline uint32_t iblt_hash_check(uint32_t key) {
    return wang_hash(key, 0xDEADBEEF);
}

// ==========================================
// Bloom Filter (Static Memory, No Malloc)
// ==========================================
template <size_t NUM_BITS, uint8_t NUM_HASHES>
class BloomFilter {
private:
    uint8_t bit_array[(NUM_BITS + 7) / 8];
public:
    BloomFilter() {
        memset(bit_array, 0, sizeof(bit_array));
    }

    void add(uint32_t item) {
        for (uint8_t i = 0; i < NUM_HASHES; i++) {
            uint32_t idx = wang_hash(item, i) % NUM_BITS;
            bit_array[idx / 8] |= (1 << (idx % 8));
        }
    }

    bool contains(uint32_t item) const {
        for (uint8_t i = 0; i < NUM_HASHES; i++) {
            uint32_t idx = wang_hash(item, i) % NUM_BITS;
            if (!(bit_array[idx / 8] & (1 << (idx % 8)))) {
                return false;
            }
        }
        return true;
    }
};

// ==========================================
// IBLT Cell
// ==========================================
// Total size: 12 bytes per cell. Extremely memory efficient.
struct IBLTCell {
    int32_t count;
    uint32_t keySum;
    uint32_t hashSum;
};

// ==========================================
// IBLT Engine
// ==========================================
template <size_t M_CELLS, uint8_t K_HASHES>
class IBLT {
public:
    IBLTCell cells[M_CELLS];

    IBLT() {
        memset(cells, 0, sizeof(cells));
    }

    void insert(uint32_t key) {
        for (uint8_t i = 0; i < K_HASHES; i++) {
            uint32_t idx = wang_hash(key, i) % M_CELLS;
            cells[idx].count += 1;
            cells[idx].keySum ^= key;
            cells[idx].hashSum ^= iblt_hash_check(key);
        }
    }

    // A - B
    void subtract(const IBLT<M_CELLS, K_HASHES>& other, IBLT<M_CELLS, K_HASHES>& result) const {
        for (size_t i = 0; i < M_CELLS; i++) {
            result.cells[i].count = this->cells[i].count - other.cells[i].count;
            result.cells[i].keySum = this->cells[i].keySum ^ other.cells[i].keySum;
            result.cells[i].hashSum = this->cells[i].hashSum ^ other.cells[i].hashSum;
        }
    }

    bool decode(uint32_t* added, size_t max_added, size_t* added_count,
                uint32_t* removed, size_t max_removed, size_t* removed_count) {
        *added_count = 0;
        *removed_count = 0;
        
        size_t pure_cells[M_CELLS];
        size_t pure_count = 0;

        // 1. Scan for initial pure cells
        for (size_t i = 0; i < M_CELLS; i++) {
            if (cells[i].count == 1 || cells[i].count == -1) {
                pure_cells[pure_count++] = i;
            }
        }

        // 2. Peel the graph
        size_t iterations = 0;
        while (pure_count > 0 && iterations < M_CELLS * 2) {
            iterations++;
            size_t i = pure_cells[--pure_count];
            
            int32_t c = cells[i].count;
            uint32_t k = cells[i].keySum;
            uint32_t h = cells[i].hashSum;
            
            if (c == 0) continue;

            // Integrity check prevents decoding poisoned cells
            if ((c == 1 || c == -1) && iblt_hash_check(k) == h) {
                if (c == 1) {
                    if (*added_count < max_added) added[(*added_count)++] = k;
                } else {
                    if (*removed_count < max_removed) removed[(*removed_count)++] = k;
                }
                
                // Remove the peeled key from its K_HASHES locations
                for (uint8_t j = 0; j < K_HASHES; j++) {
                    uint32_t idx = wang_hash(k, j) % M_CELLS;
                    cells[idx].count -= c;
                    cells[idx].keySum ^= k;
                    cells[idx].hashSum ^= h;
                    
                    if (cells[idx].count == 1 || cells[idx].count == -1) {
                        pure_cells[pure_count++] = idx;
                    }
                }
            }
        }

        // 3. Verify completeness
        for (size_t i = 0; i < M_CELLS; i++) {
            if (cells[i].count != 0 || cells[i].keySum != 0 || cells[i].hashSum != 0) {
                return false; // Decode incomplete / failed
            }
        }
        return true;
    }
};

// ==========================================
// LithoMesh Engine (The Wrapper)
// ==========================================
template <size_t M_CELLS, uint8_t K_HASHES, size_t BLOOM_BITS, uint8_t BLOOM_HASHES>
class LithoMeshEngine {
public:
    IBLT<M_CELLS, K_HASHES> iblt;
    BloomFilter<BLOOM_BITS, BLOOM_HASHES> bloom;

    LithoMeshEngine() {}

    // Logs an event. Returns false if duplicate detected.
    bool log_event(uint32_t event_id) {
        if (bloom.contains(event_id)) {
            return false; // Idempotency rejected
        }
        bloom.add(event_id);
        iblt.insert(event_id);
        return true;
    }
};

#endif // LITHOMESH_H
