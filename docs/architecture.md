# Continuity Net Architecture (Industrial IoT - Scale Edition)

## System Overview
Continuity Net is a rugged, decentralized mesh network designed for massive-scale (100,000+ nodes) subterranean and industrial deployments. It bypasses RF spectrum exhaustion using Hierarchical Clustering and Time-Division multiplexing.

## Components

### 1. Mesh Layer (`/mesh`)
- **Topology**: Mesh-of-Trees (Hierarchical Clustering). Nodes are divided into clusters of ~100.
- **Roles**:
  - **Leaf Nodes**: Standard sensors. They only communicate directly with their Cluster Head.
  - **Cluster Heads (CH)**: Powerful aggregator nodes that form the core routing backbone.
- **Physical Layer**: 
  - **TDMA**: Time-Division Multiple Access prevents intra-cluster collisions by assigning microsecond transmission slots to each leaf node.
  - **FHSS**: Frequency Hopping Spread Spectrum divides the Sub-GHz spectrum into 64 distinct channels to prevent inter-cluster cross-talk.

### 2. Synchronization Layer (`/sync`)
- **Role**: Manages distributed sensor data using Conflict-Free Replicated Data Types (CRDTs).
- **Responsibilities**: 
  - **Batched Compression**: CHs intercept and compress 10 minutes of leaf node deltas into a single backbone payload.
  - **Epoch-based compaction**: Aggressive pruning of causal history.

### 3. Gateway Layer (`/gateway`)
- **Role**: Wired surface nodes that bridge the subterranean CH backbone to the internet.

### 4. Client Applications (`/frontend`)
- **Role**: Surface-level control dashboards.

### 5. Simulation Environment (`/simulation`)
- **Role**: Cluster-Head backbone discrete event simulation for extreme scalability validation.
