# Minimum Viable Product (MVP) Scope - Industrial IoT

## Objective
Prove the viability of the Continuity Sync Protocol for uninterrupted sensor telemetry in a subterranean mining environment.

## Core Features
1. **Always-On Peer Discovery**: Dedicated hardware nodes discover each other instantly via Sub-GHz RF.
2. **Telemetry Sync**: Continuous synchronization of temperature, gas, and structural integrity sensors.
3. **Epoch Compaction**: CRDT history is aggressively pruned every 10 minutes to prevent storage ballooning.
4. **Surface Relay**: When a node reaches a wired surface gateway, the aggregated mesh state is offloaded to the central cloud.

## Exclusions (Post-MVP)
- Consumer mobile device support (Killed).
- Battery optimization (Nodes assume constant power or massive battery banks).
