import heapq
import random

class Event:
    def __init__(self, t, event_type, target, data=None):
        self.time = t
        self.event_type = event_type
        self.target = target
        self.data = data

    def __lt__(self, other):
        return self.time < other.time

class Simulator:
    def __init__(self):
        self.time = 0
        self.events = []
        self.metrics = {
            'leaf_generated': 0,
            'ch_backbone_tx': 0,
            'delivered': 0,
            'channel_collisions': 0,
            'rf_metal_reflections': 0,
            'firmware_crashes': 0,
            'retransmission_storm_drops': 0
        }
        # Track active transmissions per 64 available FHSS channels
        self.active_transmissions = {i: 0 for i in range(64)}

    def schedule(self, delay, event_type, target, data=None):
        heapq.heappush(self.events, Event(self.time + delay, event_type, target, data))

    def run(self, max_time):
        while self.events and self.time < max_time:
            event = heapq.heappop(self.events)
            self.time = event.time
            if hasattr(event.target, 'handle_event'):
                event.target.handle_event(self, event)
            elif callable(event.target):
                event.target(self, event)

class ClusterHead:
    def __init__(self, ch_id, cluster_size=100, is_gateway=False):
        self.id = ch_id
        self.is_gateway = is_gateway
        self.cluster_size = cluster_size
        self.is_online = True
        self.channel = random.randint(0, 63) # Initial FHSS Channel
        
        self.queue = []
        self.metal_reflection_drop = 0.20 
        self.firmware_bug_chance = 0.0001
        
    def handle_event(self, sim, event):
        if not self.is_online: return

        if event.event_type == 'BATCH_GENERATE':
            # TDMA ensures 100 leaf nodes perfectly transmit to CH without collisions
            sim.metrics['leaf_generated'] += self.cluster_size
            
            # CH compresses into a single backbone payload
            sim.metrics['ch_backbone_tx'] += 1
            self.queue.append({'src': self.id, 'retries': 0})
            
            sim.schedule(random.uniform(0.001, 0.01), 'TRANSMIT', self)
            
            # Batch interval: 600s (10 minutes)
            sim.schedule(random.expovariate(1.0/600.0), 'BATCH_GENERATE', self)

        elif event.event_type == 'TRANSMIT':
            if not self.queue: return
            packet = self.queue[0]
            
            if random.random() < self.firmware_bug_chance:
                sim.metrics['firmware_crashes'] += 1
                self.is_online = False
                sim.schedule(30, 'REBOOT', self)
                return

            sim.active_transmissions[self.channel] += 1
            sim.schedule(0.05, 'TRANSMIT_END', self, packet) # 50ms CH backbone transmission

        elif event.event_type == 'TRANSMIT_END':
            sim.active_transmissions[self.channel] -= 1
            packet = event.data
            
            # Inter-cluster Collision ONLY IF on same channel at same time
            if sim.active_transmissions[self.channel] > 0:
                sim.metrics['channel_collisions'] += 1
                self.handle_failure(sim, packet)
            elif random.random() < self.metal_reflection_drop:
                sim.metrics['rf_metal_reflections'] += 1
                self.handle_failure(sim, packet)
            else:
                self.queue.pop(0)
                sim.metrics['delivered'] += self.cluster_size # 100 node payload delivered
                if self.queue:
                    sim.schedule(random.uniform(0.01, 0.05), 'TRANSMIT', self)

        elif event.event_type == 'REBOOT':
            self.is_online = True
            self.queue = []

    def handle_failure(self, sim, packet):
        packet['retries'] += 1
        if packet['retries'] < 5:
            backoff = (2 ** packet['retries']) * random.uniform(0.1, 0.5)
            # Hop to a new frequency channel to avoid jamming
            self.channel = random.randint(0, 63)
            sim.schedule(backoff, 'TRANSMIT', self)
        else:
            sim.metrics['retransmission_storm_drops'] += self.cluster_size
            self.queue.pop(0)
            if self.queue:
                sim.schedule(random.uniform(0.01, 0.05), 'TRANSMIT', self)


def run_simulation(leaf_nodes=100000, duration=3600):
    sim = Simulator()
    cluster_size = 100
    num_chs = leaf_nodes // cluster_size
    
    for i in range(num_chs):
        is_gw = (i < num_chs * 0.05)
        ch = ClusterHead(i, cluster_size=cluster_size, is_gateway=is_gw)
        # Stagger initial batch generation over the first 10 minutes
        sim.schedule(random.uniform(0, 600), 'BATCH_GENERATE', ch)

    sim.run(duration)
    return sim.metrics

def main():
    scenarios = [
        10000,
        50000,
        100000
    ]
    
    print("| Leaf Nodes | CH Backbone Nodes | Leaf Generated | CH Transmitted | Leaf Delivered | Deliv % | CH Collisions | RF Drops | Storm Drops |")
    print("|---|---|---|---|---|---|---|---|---|")
    
    for nodes in scenarios:
        metrics = run_simulation(leaf_nodes=nodes, duration=3600)
        leaf_gen = metrics['leaf_generated']
        leaf_deliv = metrics['delivered']
        deliv_pct = (leaf_deliv / max(1, leaf_gen)) * 100
        chs = nodes // 100
        
        print(f"| {nodes} | {chs} | {leaf_gen} | {metrics['ch_backbone_tx']} | {leaf_deliv} | {deliv_pct:.2f}% | {metrics['channel_collisions']} | {metrics['rf_metal_reflections']} | {metrics['retransmission_storm_drops']} |")

if __name__ == "__main__":
    main()
