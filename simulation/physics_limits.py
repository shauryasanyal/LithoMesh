import math
import random
import numpy as np

print("==================================================")
print(" PHASE 0: SUBTERRANEAN PHYSICS HARD LIMITS (DEEP)")
print("==================================================\n")

# ---------------------------------------------------------
# 1. RF TUNNEL WAVEGUIDE PATH LOSS (868 MHz)
# Based on Sun & Akyildiz (2010) / Emslie et al.
# Tunnel as an oversized imperfect dielectric waveguide
# ---------------------------------------------------------
def simulate_rf_waveguide(tunnel_width_m, tunnel_height_m, freq_hz, tx_power_dbm, rx_sens_dbm):
    c = 3e8
    wavelength = c / freq_hz
    
    # Relative permittivity of typical mine rock (e.g., Limestone/Coal)
    eps_r = 5.0 
    
    # Attenuation constant for the dominant fundamental mode (E_11) in dB/m
    # Simplified Emslie model for oversized rectangular tunnel
    part_a = 1.0 / ((tunnel_width_m ** 3) * math.sqrt(eps_r - 1))
    part_b = 1.0 / ((tunnel_height_m ** 3) * math.sqrt(eps_r - 1))
    
    alpha_db_m = 4.343 * (wavelength ** 2) * (part_a + part_b)
    
    # Free space loss at a reference distance (e.g., 10m)
    d0 = 10.0
    fspl_d0 = 20 * math.log10(d0) + 20 * math.log10(freq_hz) - 147.55
    
    max_distance = 0
    # Simulate a tunnel with a 90-degree corner every 500 meters
    for d in range(10, 15000, 10):
        corners_passed = d // 500
        corner_loss = corners_passed * 25.0 # 25 dB loss per 90-degree corner
        
        shadow_fading = random.gauss(0, 6.0) # 6dB standard deviation
        path_loss = fspl_d0 + (alpha_db_m * (d - d0)) + corner_loss + shadow_fading
        
        rx_power = tx_power_dbm - path_loss
        if rx_power < rx_sens_dbm:
            max_distance = d
            break
            
    if max_distance == 0: max_distance = 15000
    return max_distance, alpha_db_m

print("--- EXPERIMENT 1: 868 MHz LoRa Tunnel Propagation ---")
w, h = 4.0, 3.0 # 4m x 3m standard drift
freq = 868e6
tx_power = 14 # 14 dBm (ETSI max for 868)
rx_sens = -120 # LoRa SF7

max_d, alpha = simulate_rf_waveguide(w, h, freq, tx_power, rx_sens)
print(f"Tunnel Dims   : {w}m x {h}m")
print(f"Attenuation   : {alpha * 100:.2f} dB / 100m")
print(f"Max Link Dist : {max_d} meters (before dropping below {rx_sens} dBm)")
if max_d < 1000:
    print("CONCLUSION    : Multi-hop dense relays required. Cannot bypass structural blockages directly.")
print("--- EXPERIMENT 3: ETSI 868 MHz Duty Cycle Limit ---")
# ETSI EN 300 220 limits 868-868.6 MHz to 1% duty cycle
allowed_tx_time_per_hour = 3600 * 0.01 # 36 seconds

# Time on Air (ToA) for a LoRa packet (SF7, 125kHz, 120 byte payload)
# Approx 200ms per packet
toa_packet_s = 0.200
packets_per_sync = 24 # 2400 bytes / 100 bytes payload (with RS parity)
sync_tx_time_s = packets_per_sync * toa_packet_s

max_syncs_per_hour = allowed_tx_time_per_hour / sync_tx_time_s

print(f"Allowed TX Time  : {allowed_tx_time_per_hour} seconds/hour (1%)")
print(f"Time-On-Air/Sync : {sync_tx_time_s:.2f} seconds")
print(f"Max Syncs/Hour   : {int(max_syncs_per_hour)}")
print(f"CONCLUSION       : Nodes cannot sustain continuous replication. Gossip rates must be strictly throttled to 1 sync every {60/int(max_syncs_per_hour):.1f} minutes.")
print()

# ---------------------------------------------------------
# 2. SEISMIC / ACOUSTIC TIME SYNC JITTER
# ---------------------------------------------------------
def simulate_seismic_jitter(distance_m, target_jitter_us=10):
    # P-wave velocity in rock (e.g., Granite/Hard rock)
    base_vp_m_s = 5000.0 
    
    # To achieve <10 us jitter, how accurately must we know the rock density?
    # T = d / Vp  => dT = -d/(Vp^2) * dVp
    # |dVp| = (|dT| * Vp^2) / d
    
    target_dt_s = target_jitter_us * 1e-6
    max_dvp = (target_dt_s * (base_vp_m_s ** 2)) / distance_m
    
    allowed_variance_pct = (max_dvp / base_vp_m_s) * 100
    
    # Simulate a realistic rock fracture/fault affecting velocity by 2%
    realistic_dvp = base_vp_m_s * 0.02
    actual_jitter_s = (distance_m / (base_vp_m_s - realistic_dvp)) - (distance_m / base_vp_m_s)
    
    return allowed_variance_pct, actual_jitter_s * 1e6

print("--- EXPERIMENT 2: Seismic Clock Sync Jitter Limit ---")
dist = 500 # 500 meters between nodes
target = 10 # 10 microsecond requirement for TDMA Glossy flooding

allowed_var, actual_jit = simulate_seismic_jitter(dist, target)
print(f"Node Distance : {dist} meters")
print(f"Target Jitter : < {target} µs")
print(f"Req. Vp Margin: ±{allowed_var:.5f}% (Impossible in geology)")
print(f"Realistic Var : 2.0% (due to micro-fractures, voids, water tables)")
print(f"Actual Jitter : {actual_jit:.2f} µs")
if actual_jit > target:
    print(f"CONCLUSION    : HARD PHYSICS FAIL. Acoustic sync misses requirement by {actual_jit/target:.1f}x.")
    print("                TDMA and Synchronous Flooding are mathematically dead on arrival.")

