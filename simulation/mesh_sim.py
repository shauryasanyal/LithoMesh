import json
import math

def simulate_mesh_network(nodes, is_offline, packet_loss=0.20, internet_ratio=0.05, duty_cycle=1.0):
    """
    Simulates Industrial IoT network conditions for Continuity Net.
    
    Parameters:
    - nodes: Total number of sensors/devices
    - is_offline: True if deeply subterranean (0% internet)
    - packet_loss: 20% (harsh RF environment: metal/concrete)
    - internet_ratio: 5% of nodes are wired gateways to the surface
    - duty_cycle: 100% (wired power or large industrial batteries)
    """
    # 1. Sync Time (s)
    base_hops = math.log10(nodes) * 3
    sleep_delay = (1 / duty_cycle) * 0.5  # 0.5s per hop with 100% duty cycle
    retransmissions = 1 / (1 - packet_loss)
    sync_time = base_hops * sleep_delay * retransmissions
    
    # 2. Success Rate (%)
    success_rate = (1 - (packet_loss ** 3)) * 100
    if is_offline:
        success_rate *= 0.95 # Highly resilient in industrial settings due to always-on nodes
        
    # 3. Storage Growth (MB/hr)
    # 60 ops/hr (1 per minute sensor read). CRDT overhead 150 bytes.
    # IMPLEMENTED PIVOT: Epoch Compaction reduces historical overhead by 95%
    ops_per_hour = 60 * nodes
    crdt_overhead_bytes = ops_per_hour * 150
    compacted_bytes = crdt_overhead_bytes * 0.05 
    storage_growth_mb = compacted_bytes / (1024 * 1024)
    
    # 4. Energy (mWh/node/hr)
    # Always on listening
    neighbors = min(40, int(nodes * 0.1)) # Denser industrial mesh
    tx_energy = 2.0 # Higher power for penetration (mWh)
    rx_energy = 0.5 # Receiving power
    energy = (60 * tx_energy) + (60 * neighbors * rx_energy) * retransmissions + (3600 * rx_energy) # baseline listening
    
    # 5. Gateway Load (req/s/gateway)
    if is_offline:
        gateway_load = 0
    else:
        gateway_load = (nodes / 60) / max(1, (nodes * internet_ratio))
        
    return {
        "Scale": f"{nodes}",
        "Condition": "Subterranean" if is_offline else "Surface Connected (5% GW)",
        "Sync Time (s)": round(sync_time, 2),
        "Success Rate (%)": round(success_rate, 2),
        "Storage Growth (MB/hr)": round(storage_growth_mb, 4),
        "Energy (mWh/hr)": round(energy, 2),
        "Gateway Load (req/s)": round(gateway_load, 2)
    }

if __name__ == "__main__":
    scenarios = [
        (100, False), (100, True),
        (1000, False), (1000, True),
        (10000, False), (10000, True)
    ]
    
    print("Running Industrial IoT Continuity Net Simulation...\n")
    print(f"{'Scale':<10} | {'Condition':<28} | {'Sync (s)':<10} | {'Success (%)':<12} | {'Storage (MB/hr)':<15} | {'Energy (mWh)':<12} | {'GW Load (req/s)'}")
    print("-" * 115)
    
    for n, offline in scenarios:
        res = simulate_mesh_network(n, offline)
        print(f"{res['Scale']:<10} | {res['Condition']:<28} | {res['Sync Time (s)']:<10} | {res['Success Rate (%)']:<12} | {res['Storage Growth (MB/hr)']:<15} | {res['Energy (mWh/hr)']:<12} | {res['Gateway Load (req/s)']}")
