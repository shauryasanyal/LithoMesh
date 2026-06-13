import time
import math

class SimulatedLink:
    def __init__(self, latency_ms, bandwidth_bps):
        self.latency_ms = latency_ms
        self.bandwidth_bps = bandwidth_bps
        self.total_bytes_sent = 0
        self.total_roundtrips = 0

    def transmit(self, bytes_count):
        self.total_bytes_sent += bytes_count
        self.total_roundtrips += 1
        # Time = Roundtrip Latency + Transmission Time
        transmission_time = (bytes_count * 8) / self.bandwidth_bps
        return (self.latency_ms * 2 / 1000.0) + transmission_time


def benchmark_merkle_vs_iblt(total_events, missing_events, latency_ms, bandwidth_bps):
    print(f"\n=======================================================")
    print(f"LINK: {latency_ms}ms latency | {bandwidth_bps} bps bandwidth")
    print(f"STATE: {total_events} total events | {missing_events} missing events ({(missing_events/total_events)*100:.1f}% divergence)")
    print(f"=======================================================")

    link_iblt = SimulatedLink(latency_ms, bandwidth_bps)
    link_merkle = SimulatedLink(latency_ms, bandwidth_bps)

    # ---------------------------------------------------------
    # 1. IBLT Sync (Single Roundtrip)
    # ---------------------------------------------------------
    # IBLT size = 1.5 * missing_events * 12 bytes per cell
    iblt_cells = int(missing_events * 1.5)
    iblt_payload = iblt_cells * 12
    
    iblt_time = link_iblt.transmit(iblt_payload) # A sends IBLT to B
    iblt_time += link_iblt.transmit(missing_events * 4) # B sends missing data back to A

    # ---------------------------------------------------------
    # 2. Merkle Tree Sync (Binary Search 20 Questions)
    # ---------------------------------------------------------
    # Depth of tree = log2(total_events)
    tree_depth = math.ceil(math.log2(total_events))
    
    # In the worst case (missing events spread out), we traverse down to the leaves.
    # We assume an optimized traversal where we send multiple hashes per level,
    # but we still suffer from back-and-forth roundtrips.
    # A conservative model: We do log2(total_events) roundtrips to find the discrepancies.
    merkle_time = 0
    
    for level in range(tree_depth):
        # Sending hashes for the current level (assume 256-bit / 32-byte hashes)
        # The number of branches to check grows with missing_events, up to the level width.
        nodes_to_check = min(missing_events, 2**level)
        merkle_payload = nodes_to_check * 32
        merkle_time += link_merkle.transmit(merkle_payload)

    # Finally send the missing data
    merkle_time += link_merkle.transmit(missing_events * 4)

    print(f"--- IBLT (LithoMesh) ---")
    print(f"Roundtrips : {link_iblt.total_roundtrips}")
    print(f"Payload TX : {link_iblt.total_bytes_sent} bytes")
    print(f"Total Time : {iblt_time:.2f} seconds")

    print(f"\n--- Merkle Tree (Standard) ---")
    print(f"Roundtrips : {link_merkle.total_roundtrips}")
    print(f"Payload TX : {link_merkle.total_bytes_sent} bytes")
    print(f"Total Time : {merkle_time:.2f} seconds")
    
    print("\nCONCLUSION:")
    if iblt_time < merkle_time:
        speedup = merkle_time / iblt_time
        print(f"🏆 LithoMesh is {speedup:.1f}x faster.")
    else:
        speedup = iblt_time / merkle_time
        print(f"❌ Merkle is {speedup:.1f}x faster.")

if __name__ == '__main__':
    # Scenario 1: BLE (Low latency, high bandwidth)
    benchmark_merkle_vs_iblt(total_events=10000, missing_events=50, latency_ms=50, bandwidth_bps=1000000)
    
    # Scenario 2: LoRa (High latency, low bandwidth)
    benchmark_merkle_vs_iblt(total_events=10000, missing_events=50, latency_ms=1000, bandwidth_bps=10000)
