# Development Roadmap

## Phase 1: Core Mesh & Sync (Months 1-2)
- Implement base CRDT structures in `/sync`.
- Develop mDNS discovery and local TCP transports in `/mesh`.
- Establish basic testing in `/simulation`.

## Phase 2: Gateway & Infrastructure (Months 3-4)
- Build the internet bridge in `/gateway`.
- Implement secure tunneling and WebSocket relays.
- Add authentication and authorization handling.

## Phase 3: Client Integration (Months 5-6)
- Develop the `/frontend` web application leveraging local mesh connectivity.
- Develop the `/mobile` application with BLE proximity networking.

## Phase 4: Hardening & Scale (Months 7+)
- Optimize battery usage for mobile BLE mesh.
- Expand `/simulation` for thousands of concurrent nodes.
- Conduct security audits on the sync protocol.
