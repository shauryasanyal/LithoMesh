import random
import hashlib
import struct
from week1_sync_benchmark import IBLT

# ==========================================
# 1. Idempotency Layer: Bloom Filter
# ==========================================
class BloomFilter:
    def __init__(self, size=1000, num_hashes=3):
        self.size = size
        self.num_hashes = num_hashes
        self.bit_array = [False] * size

    def _hash(self, item, seed):
        h = hashlib.sha256(struct.pack('<II', item, seed)).digest()
        return int.from_bytes(h[:4], 'little') % self.size

    def add(self, item):
        for i in range(self.num_hashes):
            self.bit_array[self._hash(item, i)] = True

    def __contains__(self, item):
        for i in range(self.num_hashes):
            if not self.bit_array[self._hash(item, i)]:
                return False
        return True

# ==========================================
# 2. Resilient IBLT Engine
# ==========================================
class ResilientEngine:
    def __init__(self, m_cells):
        self.iblt = IBLT(m_cells)
        # Size Bloom filter aggressively to ensure very low false-positive rate
        self.bloom = BloomFilter(size=m_cells * 20, num_hashes=4)
        self.log = []

    def log_event(self, event_id):
        if event_id in self.bloom:
            # Idempotency guard triggered: Ignore duplicate
            return False
        
        self.bloom.add(event_id)
        self.iblt.insert(event_id)
        self.log.append(event_id)
        return True

# ==========================================
# 3. Forward Error Correction (FEC) Simulator
# ==========================================
def simulate_fec_transfer(data_blocks, fec_redundancy_ratio, loss_rate):
    """
    Simulates sending blocks over a lossy LoRa link with FEC redundancy.
    Using erasure codes (like Reed-Solomon), if the number of received packets
    is >= the original number of packets, we can reconstruct everything.
    """
    total_blocks_sent = int(len(data_blocks) * (1 + fec_redundancy_ratio))
    blocks_received = int(total_blocks_sent * (1 - loss_rate))
    
    if blocks_received >= len(data_blocks):
        return data_blocks, True
    else:
        return None, False

# ==========================================
# 4. Tests
# ==========================================
def test_idempotency_fix():
    print("=========================================================")
    print("--- Test 1: Bloom Filter Guard vs Duplicates ---")
    print("=========================================================")
    engine_A = ResilientEngine(100)
    engine_B = ResilientEngine(100)
    
    for i in range(1, 51):
        engine_A.log_event(i)
        engine_B.log_event(i)
        
    was_logged = engine_A.log_event(10)
    print(f"Node A attempted to log duplicate event 10.")
    print(f"Was duplicate inserted into IBLT? {was_logged}")
    
    delta = engine_A.iblt.subtract(engine_B.iblt)
    success, added, _ = delta.decode()
    print(f"Decode Success: {success}")
    print(f"Did the duplicate trap the engine? {not success}")

def test_fec_transfer():
    print("\n=========================================================")
    print("--- Test 2: FEC Transfer vs LoRa Packet Loss ---")
    print("=========================================================")
    iblt = IBLT(100)
    for i in range(50): iblt.insert(i)
    
    chunks = iblt.cells
    redundancy = 0.30 # 30% parity data
    losses = [0.05, 0.15, 0.25, 0.40]
    
    for loss in losses:
        _, success = simulate_fec_transfer(chunks, redundancy, loss)
        status = "PASS" if success else "FAIL"
        print(f"Network Loss {int(loss*100):02d}% | FEC Parity {int(redundancy*100)}% -> Recovery: {status}")

if __name__ == '__main__':
    test_idempotency_fix()
    test_fec_transfer()
