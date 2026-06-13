# LithoMesh: Feasibility of Subterranean Mesh Sync & Reconciliation

**LithoMesh** is an experimental systems research project investigating the boundaries of underground wireless sensor networks (WSNs). Specifically, it aggressively tests the feasibility of combining **Seismic/Acoustic Clock Synchronization**, **Synchronous Flooding (Glossy)**, and **Fixed-Memory IBLT-based CRDT State Reconciliation** in harsh, offline environments.

**Status:** Research & Validation Phase  
**Target Publication Venues:** IPSN, EWSN, SenSys, IEEE IoT-J

> **Note:** This project is explicitly **not** designed for commercial deployment. Strict regulatory limits (e.g., ETSI 1% duty cycle for 868MHz), underground physics (RF multi-path, acoustic jitter), and fundamental mathematical bounds of static IBLTs present hard limits to commercial viability. This repository exists to document exactly *where* and *why* these systems fail, and under what narrow envelopes they succeed.

---

## 🔬 Core Research Hypotheses & Findings

### 1. Bounded-Memory CRDT State Sync via Fixed IBLT
CRDTs accumulate tombstones indefinitely, destroying memory on embedded devices during prolonged offline periods. LithoMesh attempts to use a fixed-memory Invertible Bloom Lookup Table (IBLT) to act as a garbage-collected summary of state.
*   **Current Finding:** Implemented template-based static C++ engine requiring `1,328 bytes` of RAM. Tested over simulated LoRa with Reed-Solomon FEC framing.
*   **Mathematical Boundary:** An IBLT of $M$ cells can successfully peel a divergence $D$ only if $M > 1.5 \times D$. If $D$ exceeds this threshold (e.g., extended offline state generation), the decode fails gracefully. 

### 2. Seismic/Acoustic Clock Synchronization
Can a physical acoustic pulse (e.g., a hammer strike on rock) provide <10 µs jitter time-sync for underground nodes without GPS?
*   **Hypothesis Limit:** If acoustic jitter exceeds >10 µs, Time Division Multiple Access (TDMA) and synchronous flooding collapse. 

### 3. Global Synchronous Flooding via Constructive Interference
Extending the Glossy architecture (Ferrari et al., IPSN'11) to underground environments where multi-path reflections and severe attenuation dominate.

---

## 🛣️ Research Roadmap

### Phase 0: The Hard Stops (Current)
*   **IBLT Math:** Formally prove when fixed IBLT bounds succeed (e.g., $D < M/1.22$ for $K=3$).
*   **Regulatory & Propagation:** Replicate Vuran/Akyildiz underground 868 MHz path-loss models in simulation.

### Phase 1: Lab Prototypes
*   **Acoustic Sync Testbed:** Build hammer-pulse experiment. Measure sync jitter on 3 nodes via accelerometers. *(Gate: <10 µs jitter required).*
*   **Glossy on ESP32:** Port synchronous flooding to ESP32. Chart delivery ratio vs. alignment error.
*   **Piezo Harvester Bench:** Measure µW output under mine-spectrum vibrations.

### Phase 2: Simulation & Field
*   **NS3 Simulation:** Model 500-1000 nodes with real underground propagation models.
*   **IBLT Emulation:** Simulate 72h divergence within a fixed 2 MB IBLT and observe failure cascading.

### Phase 3: Targeted Publications
1.  *Seismic Time Synchronization for Underground Wireless Sensor Networks* (Focusing on why acoustic sync cannot achieve sub-microsecond precision).
2.  *Fixed-Memory IBLT-Based CRDT State Reconciliation: Feasibility and Limits* (Systems paper).
3.  *Level 4 Gauntlet: Embedded IBLT Reconciliation Engine Validation Framework* (Toolkit paper based on this repository).

---

## 🛠️ The Level 4 Gauntlet (IBLT Validation Framework)

This repository houses the **Level 4 Gauntlet**, an embedded framework for validating IBLT reconciliation over simulated noisy transports.

*   **Test A (Healthy):** Proves sublinear CPU scaling (Decode Iterations).
*   **Test B (Loss/FEC):** Proves Reed-Solomon error correction mathematically recovers 30% byte loss without triggering naive round-trip NACKs.
*   **Test C (Threshold Exceedance):** Deliberately explodes the mathematics to prove the $M > 1.5 \times D$ failure boundary.
*   **Test D (Soak):** 100 consecutive syncs proving 0-byte heap leakage due to pure static allocation.

*Run the framework:* `python simulation/level4_gauntlet.py` and `python simulation/fec_integration_test.py`
