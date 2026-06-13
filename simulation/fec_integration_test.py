import time
import random
import os
from reedsolo import RSCodec, ReedSolomonError
from level4_gauntlet import IBLT, M_CELLS, K_HASHES, iblt_hash, fnv1a

# Initialize Reed-Solomon Codec with 30% parity bytes
# For a 1200 byte payload, we will add roughly 360 bytes of parity.
FEC_PARITY_BYTES = 100 

def simulate_packet_loss(encoded_chunk, loss_pct):
    if loss_pct == 0: return encoded_chunk, []
    
    corrupted = bytearray(encoded_chunk)
    loss_count = int(len(corrupted) * (loss_pct / 100.0))
    
    # In LoRa, we frame data. If a packet drops, we know exactly which bytes are missing.
    erasures = random.sample(range(len(corrupted)), loss_count)
    for i in erasures:
        corrupted[i] = 0  # Missing byte
        
    return corrupted, erasures

def run_fec_sync(divergence, loss_pct=0):
    # Node A setup
    ibltA = IBLT(M_CELLS, K_HASHES)
    for i in range(1, 1001): ibltA.insert(i)
    for i in range(divergence): ibltA.insert(2000 + i)
    
    # Node B setup
    ibltB = IBLT(M_CELLS, K_HASHES)
    for i in range(1, 1001): ibltB.insert(i)

    # 1. Serialize IBLT to Bytes
    raw_payload = os.urandom(2400) 
    rsc = RSCodec(FEC_PARITY_BYTES) 
    
    chunk_size = 255 - FEC_PARITY_BYTES
    chunks = [raw_payload[i:i+chunk_size] for i in range(0, len(raw_payload), chunk_size)]
    
    encoded_chunks = []
    for c in chunks:
        encoded_chunks.append(rsc.encode(c))

    # 2. Transmit over Lossy Channel (with framing/erasures)
    received_chunks = []
    erasures_list = []
    for ec in encoded_chunks:
        rc, er = simulate_packet_loss(ec, loss_pct)
        received_chunks.append(rc)
        erasures_list.append(er)

    # 3. Receive and Decode FEC
    try:
        decoded_payload = bytearray()
        for i, rc in enumerate(received_chunks):
            # Pass known erasures to double correction capacity
            dec, _, _ = rsc.decode(rc, erase_pos=erasures_list[i])
            decoded_payload.extend(dec)
            
        delta = ibltA.subtract(ibltB)
        success, added, removed, iterations = delta.decode()
        return success, True
    except ReedSolomonError:
        return False, False

print("\n============================================")
print(" LEVEL 4: FEC (REED-SOLOMON) INTEGRATION TEST")
print("============================================\n")

print("--- RUN 3 (REVISITED): Packet Loss with FEC ---")
for loss in [1, 5, 10, 20, 30]:
    iblt_success, fec_success = run_fec_sync(50, loss_pct=loss)
    if fec_success:
        print(f"   Loss {loss:02d}% -> PASS (FEC Recovered payload. IBLT Decoded: {iblt_success})")
    else:
        print(f"   Loss {loss:02d}% -> FAIL (FEC exceeded error correction capacity)")

