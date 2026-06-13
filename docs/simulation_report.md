# Continuity Net: Simulation & Scalability Report

## Executive Summary
This report analyzes the performance of the Continuity Sync Protocol (CSP) under strict real-world constraints. The simulation models a decentralized mesh network scaling from 100 to 10,000 devices. 

## Simulation Parameters
*   **Scales Tested**: 100, 1,000, and 10,000 nodes.
*   **Network Conditions**: 
    *   **Packet Loss**: 15% (simulating noisy RF environments).
    *   **Battery Limits**: 10% Duty Cycle (Devices sleep 90% of the time to save power).
*   **Connectivity Profiles**:
    *   **Partial Internet**: 1% of devices act as active Gateways to the broader internet.
    *   **Offline**: 0% internet availability; purely localized mesh routing.
*   **Data Profile**: Each user performs 10 operations per hour; CRDT payload overhead is ~150 bytes per operation.

## Simulation Results

| Scale | Condition | Sync Time (s) | Success Rate (%) | Storage Growth (MB/hr) | Energy (mWh/node/hr) | Gateway Load (req/s) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 100 devices | Partial Internet (1% GW) | 35.29 | 99.66 | 0.143 | 72.35 | 1.67 |
| 100 devices | Offline | 35.29 | 84.71 | 0.143 | 72.35 | 0.00 |
| 1000 devices | Partial Internet (1% GW) | 52.94 | 99.66 | 1.431 | 171.18 | 1.67 |
| 1000 devices | Offline | 52.94 | 84.71 | 1.431 | 171.18 | 0.00 |
| 10000 devices | Partial Internet (1% GW) | 70.59 | 99.66 | 14.305 | 171.18 | 1.67 |
| 10000 devices | Offline | 70.59 | 84.71 | 14.305 | 171.18 | 0.00 |

## Key Findings & Bottlenecks

### 1. Sync Time & Battery Trade-offs
Because devices sleep 90% of the time, the sync time scales logarithmically but is fundamentally bottlenecked by the "wake-up" alignment of nodes. At 10,000 devices, a state delta takes ~70 seconds to fully propagate across the mesh.

### 2. Mesh Fragmentation (Success Rate)
In the purely offline scenarios, the success rate drops from 99.6% to ~84.7%. This implies that without gateway nodes acting as stable bridges, the mesh fragments into smaller, disconnected sub-graphs. 

### 3. Storage Growth (CRDT Overhead)
At 10,000 devices, local CRDT metadata balloons by ~14.3 MB per hour across the network. Over a week, this becomes unsustainable for mobile devices.
**Action Item**: We must implement CRDT "tombstone pruning" and state compaction algorithms in Phase 2.

### 4. Gateway Resiliency
Gateway load remains constant at 1.67 requests/sec per gateway, proving that routing traffic through a dynamic 1% subset of connected peers scales effectively. However, this assumes perfect load-balancing, which will require intelligent gateway discovery mechanisms in `/mesh`.
