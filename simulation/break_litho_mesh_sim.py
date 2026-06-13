import math
from iblt_crdt_demo import IBLT

def test_seismic_delay_destruction():
    print("=========================================================")
    print("--- 1. Testing Seismic Clock Propagation Delay        ---")
    print("=========================================================")
    speed_of_sound = 5000.0  # m/s in granite
    speed_of_light = 300000000.0 # m/s
    
    # Distance from the seismic impact source
    dist_A = 100.0 # Node A is 100 meters away
    dist_B = 150.0 # Node B is 150 meters away
    
    time_A_hears_tick = dist_A / speed_of_sound
    time_B_hears_tick = dist_B / speed_of_sound
    
    print(f"Node A hears seismic tick at {time_A_hears_tick*1000:.2f} ms")
    print(f"Node B hears seismic tick at {time_B_hears_tick*1000:.2f} ms")
    
    # Delay between A and B triggering their "simultaneous" transmission
    delta_t = abs(time_A_hears_tick - time_B_hears_tick)
    print(f"Time difference between A and B triggering: {delta_t*1000000:.2f} microseconds")
    
    # Constructive Interference in 802.15.4 (250kbps) requires delta_t < 0.5 microseconds.
    # Otherwise, you get Intersymbol Interference (ISI) and the signals destroy each other.
    threshold_us = 0.5
    
    if delta_t * 1000000 > threshold_us:
        print("\n[CRITICAL FAILURE]: DESTRUCTIVE INTERFERENCE")
        print("Sound is 60,000x slower than light. The seismic clock creates massive geographic timing skew.")
        print(f"Node B transmits 10,000 microseconds AFTER Node A.")
        print("They do NOT constructively interfere. They will completely jam each other's packets.")
        print("The Litho-Mesh RF model collapses.")

def test_iblt_shatter():
    print("\n=========================================================")
    print("--- 2. Testing IBLT Capacity Overload                 ---")
    print("=========================================================")
    M_CELLS = 100 # An IBLT sized to handle up to ~75 differences
    
    print("Test A: Partition creates 50 differences (Under Capacity)")
    iblt_A = IBLT(M_CELLS)
    iblt_B = IBLT(M_CELLS)
    for i in range(50):
        iblt_A.insert(i, 1)
        
    delta_50 = iblt_A.subtract(iblt_B)
    success_50, added_50, _ = delta_50.decode()
    print(f"Result: Success={success_50}, Recovered={len(added_50)} / 50 keys")
    
    print("\nTest B: Partition creates 95 differences (Over Capacity)")
    iblt_A = IBLT(M_CELLS)
    iblt_B = IBLT(M_CELLS)
    for i in range(95):
        iblt_A.insert(i, 1)
        
    delta_95 = iblt_A.subtract(iblt_B)
    success_95, added_95, _ = delta_95.decode()
    print(f"Result: Success={success_95}, Recovered={len(added_95)} / 95 keys")
    
    if not success_95 and len(added_95) == 0:
        print("\n[CRITICAL FAILURE]: CRDT STATE DEADLOCK")
        print("If a network partition lasts too long and differences exceed the fixed IBLT capacity by even 1%,")
        print("the XOR peeling graph becomes cyclic. The algorithm jams completely.")
        print("Zero data is recovered. The CRDTs are permanently permanently out of sync.")

if __name__ == '__main__':
    test_seismic_delay_destruction()
    test_iblt_shatter()
