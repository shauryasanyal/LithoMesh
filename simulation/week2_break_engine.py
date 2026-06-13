import random
from week1_sync_benchmark import IBLT

def test_delta_explosion():
    print("=========================================================")
    print("--- Test 1: Delta Explosion (The Crossover Point) ---")
    print("=========================================================")
    TOTAL = 10000
    missing_rates = [50, 500, 1000, 2000, 5000, 9000]
    
    print(f"{'Missing':<10} | {'Hash TX (B)':<15} | {'IBLT TX (B)':<15} | {'Winner':<10}")
    for missing in missing_rates:
        synced = TOTAL - missing
        
        # Hash TX: Node B sends hashes of everything it has
        hash_tx = synced * 4
        
        # IBLT TX: M_CELLS must be sized ~1.5x the expected differences to decode successfully
        m_cells = int(missing * 1.5)
        iblt_tx = m_cells * 24
        
        winner = "IBLT" if iblt_tx < hash_tx else "HASH"
        print(f"{missing:<10} | {hash_tx:<15} | {iblt_tx:<15} | {winner:<10}")

def test_duplicates():
    print("\n=========================================================")
    print("--- Test 2: Duplicate Events (XOR Vulnerability) ---")
    print("=========================================================")
    iblt_A = IBLT(100)
    iblt_B = IBLT(100)
    
    # Both have events 1-50
    for i in range(1, 51):
        iblt_A.insert(i)
        iblt_B.insert(i)
        
    # A accidentally processes event 10 a second time
    iblt_A.insert(10)
    
    delta = iblt_A.subtract(iblt_B)
    success, added, _ = delta.decode()
    print(f"Node A got duplicate event 10. Decode Success: {success}")
    if 10 in added:
        print("Recovered event 10 correctly.")
    else:
        print("[CRITICAL FLAW]: Because IBLT uses XOR for keys, inserting twice cancels the key out!")
        print("Event 10 disappeared. The IBLT thinks no differences exist. Data is lost.")

def test_packet_loss():
    print("\n=========================================================")
    print("--- Test 3: Packet Loss on IBLT Transfer ---")
    print("=========================================================")
    losses = [0.01, 0.10, 0.30, 0.50]
    for loss in losses:
        iblt_A = IBLT(100)
        iblt_B = IBLT(100)
        
        for i in range(50): iblt_A.insert(i) # A has 0-49
        for i in range(10, 50): iblt_B.insert(i) # B missing 0-9
            
        # B sends IBLT to A, but packets drop over LoRa
        iblt_B_received = IBLT(100)
        for i in range(100):
            if random.random() > loss:
                iblt_B_received.cells[i] = list(iblt_B.cells[i])
                
        delta = iblt_A.subtract(iblt_B_received)
        success, added, _ = delta.decode()
        print(f"Loss {int(loss*100)}%: Decode Success={success}, Recovered={len(added)}/10")

def test_corruption():
    print("\n=========================================================")
    print("--- Test 4: Malformed IBLT (Graceful Failure) ---")
    print("=========================================================")
    corruptions = [0.01, 0.05, 0.10, 0.20]
    
    for corp in corruptions:
        iblt_A = IBLT(100)
        iblt_B = IBLT(100)
        
        for i in range(20): iblt_A.insert(i)
        for i in range(10, 20): iblt_B.insert(i)
            
        iblt_B_rx = IBLT(100)
        for i in range(100):
            iblt_B_rx.cells[i] = list(iblt_B.cells[i])
            if random.random() < corp:
                # Simulate bit flip in transmission
                iblt_B_rx.cells[i][1] ^= 0xFFFFFFFF
                
        delta = iblt_A.subtract(iblt_B_rx)
        success, added, _ = delta.decode()
        graceful = not success
        print(f"Corruption {int(corp*100):02d}%: Decode Success={success}, Recovered={len(added):02d}. Graceful Failure? {graceful}")

if __name__ == '__main__':
    test_delta_explosion()
    test_duplicates()
    test_packet_loss()
    test_corruption()
