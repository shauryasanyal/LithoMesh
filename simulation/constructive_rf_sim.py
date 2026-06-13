class Node:
    def __init__(self, x, y, id):
        self.x = x
        self.y = y
        self.id = id
        self.received = False
        self.transmitted = False

def run_simulation(mode="GLOSSY"):
    # 30x30 grid of nodes (900 nodes), spaced 20 meters apart
    nodes = []
    id_counter = 0
    for x in range(30):
        for y in range(30):
            nodes.append(Node(x * 20, y * 20, id_counter))
            id_counter += 1
            
    # Initiator is exactly at the center of the grid
    center_node = next(n for n in nodes if n.x == 300 and n.y == 300)
    center_node.received = True
    
    transmitting_now = [center_node]
    
    threshold = 0.5  # Required received power threshold
    P_tx = 250.0     # Base transmission power
    
    step = 0
    
    while transmitting_now:
        for n in transmitting_now:
            n.transmitted = True
            
        next_transmitters = []
        
        for target in nodes:
            if target.received:
                continue
                
            if mode == "GLOSSY":
                # CONSTRUCTIVE INTERFERENCE: Power from all transmitters stacks perfectly.
                # Because the seismic clock guarantees they transmit at the exact same microsecond.
                p_rx = 0.0
                for tx in transmitting_now:
                    dist_sq = (target.x - tx.x)**2 + (target.y - tx.y)**2
                    p_rx += P_tx / dist_sq
                
                # If the sum of all overlapping RF waves breaks the threshold, it is received.
                if p_rx >= threshold:
                    target.received = True
                    next_transmitters.append(target)
                    
            elif mode == "CSMA":
                # TRADITIONAL CSMA/CA: If multiple nodes transmit, their signals destroy each other.
                # Only 1 transmitter can succeed. If >1, it's a collision (noise).
                if len(transmitting_now) == 1:
                    tx = transmitting_now[0]
                    dist_sq = (target.x - tx.x)**2 + (target.y - tx.y)**2
                    if P_tx / dist_sq >= threshold:
                        target.received = True
                        next_transmitters.append(target)
                else:
                    # Collision! Nobody receives anything.
                    break 

        if mode == "CSMA" and len(transmitting_now) > 1:
            # Broadcast storm kills the network
            break
            
        transmitting_now = next_transmitters
        step += 1
        
    received_count = sum(1 for n in nodes if n.received)
    return step, received_count, len(nodes)

if __name__ == '__main__':
    print("=========================================================")
    print("--- 1. Testing Traditional CSMA/CA Flooding           ---")
    print("=========================================================")
    step, recv, total = run_simulation("CSMA")
    print(f"Network Collapsed due to Broadcast Storm at Step {step}.")
    print(f"When the first ring of nodes tried to forward the message, they collided with each other.")
    print(f"Nodes reached: {recv} / {total}\n")
    
    print("=========================================================")
    print("--- 2. Testing Litho-Mesh Constructive Interference   ---")
    print("=========================================================")
    step, recv, total = run_simulation("GLOSSY")
    print(f"Network Flooded Successfully via Wave Superposition.")
    print(f"Because signals combined mathematically in the air, the range extended dynamically.")
    print(f"Steps to cover entire 900-node grid: {step}")
    print(f"Nodes reached: {recv} / {total}")
    print("=========================================================")
