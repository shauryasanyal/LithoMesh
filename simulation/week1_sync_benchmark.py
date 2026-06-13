import time
import struct
import hashlib

# ==========================================
# 1. Mock Event Data Structure
# ==========================================
class Event:
    def __init__(self, key, payload_bytes):
        self.key = key             # 4 byte integer (simulate 16-byte UUID in size math)
        self.payload = payload_bytes
        
    def size(self):
        return 16 + len(self.payload) # 16 byte ID + Payload

# ==========================================
# 2. Optimized IBLT Implementation
# ==========================================
class IBLT:
    def __init__(self, m, k=3):
        self.m = m
        self.k = k
        # Cell: [count, keySum, hashSum]
        self.cells = [[0, 0, 0] for _ in range(m)]

    def _hash(self, key, seed):
        h = hashlib.sha256(struct.pack('<II', key, seed)).digest()
        return int.from_bytes(h[:4], 'little')

    def _hash_check(self, key):
        h = hashlib.sha256(struct.pack('<I', key)).digest()
        return int.from_bytes(h[:4], 'little')

    def insert(self, key):
        for i in range(self.k):
            idx = self._hash(key, i) % self.m
            self.cells[idx][0] += 1
            self.cells[idx][1] ^= key
            self.cells[idx][2] ^= self._hash_check(key)

    def subtract(self, other):
        res = IBLT(self.m, self.k)
        for i in range(self.m):
            res.cells[i][0] = self.cells[i][0] - other.cells[i][0]
            res.cells[i][1] = self.cells[i][1] ^ other.cells[i][1]
            res.cells[i][2] = self.cells[i][2] ^ other.cells[i][2]
        return res

    def decode(self):
        added, removed = set(), set()
        pure_cells = [i for i in range(self.m) if abs(self.cells[i][0]) == 1]

        iterations = 0
        while pure_cells and iterations < 10000:
            iterations += 1
            i = pure_cells.pop()
            count, keySum, hashSum = self.cells[i]
            
            if count == 0: continue
                
            if abs(count) == 1 and self._hash_check(keySum) == hashSum:
                if count == 1: added.add(keySum)
                else: removed.add(keySum)
                
                for j in range(self.k):
                    idx = self._hash(keySum, j) % self.m
                    self.cells[idx][0] -= count
                    self.cells[idx][1] ^= keySum
                    self.cells[idx][2] ^= hashSum
                    if abs(self.cells[idx][0]) == 1:
                        pure_cells.append(idx)
        
        success = all(c[0] == 0 and c[1] == 0 and c[2] == 0 for c in self.cells)
        return success, added, removed

    def size_bytes(self):
        # count(4) + keySum(16 UUID) + hashSum(4) = 24 bytes per cell
        return self.m * 24

# ==========================================
# 3. Benchmark Suite
# ==========================================
def run_benchmark():
    TOTAL_EVENTS = 10000
    SYNCED_EVENTS = 9950
    PAYLOAD_SIZE = 64 # Bytes
    
    print("==================================================")
    print(f"WEEK 1: OFFLINE SYNC BENCHMARK (A: {TOTAL_EVENTS}, B: {SYNCED_EVENTS})")
    print("==================================================\n")
    
    # ------------------------------------------------
    # BASELINE 1: Full State Transfer
    # ------------------------------------------------
    print("--- Baseline 1: Full State Transfer ---")
    start = time.perf_counter()
    
    # Node A sends all events
    bytes_tx = TOTAL_EVENTS * (16 + PAYLOAD_SIZE)
    recovered = TOTAL_EVENTS - SYNCED_EVENTS
    
    cpu_time = time.perf_counter() - start
    print(f"Bytes Transferred : {bytes_tx} B")
    print(f"CPU Time          : {cpu_time*1000:.3f} ms")
    print(f"Recovered Items   : {recovered}\n")
    
    # ------------------------------------------------
    # BASELINE 2: Hash Exchange (Naive Delta)
    # ------------------------------------------------
    print("--- Baseline 2: Hash Exchange ---")
    start = time.perf_counter()
    
    # Node B sends hashes of its 950 events (4 bytes each)
    hash_tx_bytes = SYNCED_EVENTS * 4
    
    # Node A computes missing, sends 50 full events
    payload_tx_bytes = (TOTAL_EVENTS - SYNCED_EVENTS) * (16 + PAYLOAD_SIZE)
    bytes_tx_hash = hash_tx_bytes + payload_tx_bytes
    
    cpu_time = time.perf_counter() - start
    print(f"Bytes Transferred : {bytes_tx_hash} B")
    print(f"CPU Time          : {cpu_time*1000:.3f} ms")
    print(f"Recovered Items   : {recovered}\n")
    
    # ------------------------------------------------
    # CANDIDATE: IBLT Sublinear Reconciliation
    # ------------------------------------------------
    print("--- Candidate: IBLT Sync ---")
    start_cpu = time.perf_counter()
    
    # We expect ~50 differences. Capacity m=75 is safe.
    M_CELLS = 75 
    
    # Build IBLTs
    iblt_A = IBLT(M_CELLS)
    iblt_B = IBLT(M_CELLS)
    for i in range(TOTAL_EVENTS): iblt_A.insert(i)
    for i in range(SYNCED_EVENTS): iblt_B.insert(i)
        
    # Node B sends IBLT to A
    iblt_tx_bytes = iblt_B.size_bytes()
    
    # Node A subtracts and decodes
    delta = iblt_A.subtract(iblt_B)
    success, missing_keys, _ = delta.decode()
    
    # Node A sends the missing payloads
    payload_tx_bytes = len(missing_keys) * (16 + PAYLOAD_SIZE)
    bytes_tx_iblt = iblt_tx_bytes + payload_tx_bytes
    
    cpu_time = time.perf_counter() - start_cpu
    print(f"Bytes Transferred : {bytes_tx_iblt} B")
    print(f"CPU Time          : {cpu_time*1000:.3f} ms")
    print(f"Recovered Items   : {len(missing_keys)}\n")
    
    # ------------------------------------------------
    # RESULTS
    # ------------------------------------------------
    print("==================================================")
    print("FINAL METRICS")
    print("==================================================")
    print(f"Full State vs IBLT : {((bytes_tx - bytes_tx_iblt)/bytes_tx)*100:.1f}% Bandwidth Reduction")
    print(f"Hash Exch. vs IBLT : {((bytes_tx_hash - bytes_tx_iblt)/bytes_tx_hash)*100:.1f}% Bandwidth Reduction")
    print(f"Did Candidate win? : {'YES' if bytes_tx_iblt < (bytes_tx_hash * 0.5) else 'NO'} (>50% less transfer)")

if __name__ == '__main__':
    run_benchmark()
