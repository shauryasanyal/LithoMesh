# Continuity Sync Protocol (CSP) - v2.0 (High Scale)

## 1. Overview
CSP v2 is a hierarchical sync protocol optimized to eliminate CSMA/CA RF spectrum exhaustion across 100,000+ nodes.

## 2. Transport Mechanisms
- **Intra-Cluster (Leaf to CH)**: TDMA over Sub-GHz. Leaf nodes are assigned strict 10ms transmission slots by the Cluster Head. Collisions are mathematically impossible within a cluster.
- **Inter-Cluster (CH to CH/Gateway)**: CSMA/CA over FHSS. Cluster Heads communicate across 64 dynamically assigned frequency channels.

## 3. Data Synchronization (Batched CRDT)
- **Local Buffering**: Leaf nodes buffer sensor telemetry locally for 10 minutes.
- **Aggregation**: Leaf nodes transmit their 10-minute buffer to the CH. The CH deduplicates and squashes the CRDT vectors.
- **Backbone Routing**: The CH forwards a single, highly-compressed payload across the backbone to the Gateway.

## 4. Message Formats
Messages utilize dense Protocol Buffer encoding with dictionary compression for repeated sensor string keys.
