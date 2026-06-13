# LithoMesh

**LithoMesh** is an experimental offline-first edge database engine designed for extreme environments (disaster zones, rural telemetry, underground mining). It minimizes synchronization overhead after long network partitions using a bounded-memory reconciliation protocol.

## The Problem
When devices in a low-bandwidth mesh network (like LoRaWAN) lose connection for hours or days, they accumulate thousands of local events. When they briefly reconnect, traditional synchronization requires transmitting full database logs or massive lists of hashes to discover what changed. Over a 10 kbps radio link, this "discovery phase" can take minutes and drain battery life.

## The Solution: Severable Edge Database
LithoMesh bypasses traditional discovery by using an **Invertible Bloom Lookup Table (IBLT)**. 

No matter how many events are stored locally, the offline synchronization payload is statically bounded by the *estimated difference* between the nodes, not the size of their histories. 

* **Sublinear Reconciliation:** Syncing 50 missing events out of a 10,000-event log takes only **5.8 KB** of bandwidth, compared to **43.8 KB** using traditional hash exchange.
* **Idempotency Guard:** A lightweight Bloom Filter sits above the IBLT engine to mathematically reject duplicate events before they corrupt the XOR payload.
* **Forward Error Correction (FEC):** IBLT payloads are chunked with Reed-Solomon erasure coding, allowing perfect decoding even if LoRa packets are dropped in transit.

## Engineering Philosophy
1. **Physics First:** If the math requires breaking ETSI duty cycle laws, or relies on perfect environmental propagation, the architecture is invalid. 
2. **Bounded Resources:** Everything must run within the constraints of an ESP32 microcontroller powered by a battery. 
3. **Opportunistic Sync:** Nodes should function perfectly in isolation, and heal their shared state gracefully the millisecond they see a peer.
