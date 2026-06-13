#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "LithoMesh.h"
#include "LithoMeshFEC.h"

// ==============================================================
// CONFIGURATION
// Node A acts as Server (Transmitter)
// Node B acts as Client (Receiver)
// ==============================================================
#define ROLE_NODE_A true

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define DATA_BLOCKS 20
#define BLOCK_SIZE 120

struct FrameHeader {
    uint8_t magic[2];
    uint16_t session_id;
    uint8_t seq_id;
} __attribute__((packed));

struct FrameTrailer {
    uint16_t crc;
} __attribute__((packed));

LithoMeshEngine<200, 3, 2000, 4> localNode;

// Static Memory
static uint8_t raw_payload[2400];
static uint8_t data_blocks[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t parity_block[BLOCK_SIZE];
static bool packet_received[DATA_BLOCKS + 1];
static uint8_t recovered_data[DATA_BLOCKS][BLOCK_SIZE];
static uint8_t received_parity[BLOCK_SIZE];
static uint8_t reconstructed_payload[2400];
static uint32_t added_buf[200], removed_buf[200];

uint16_t current_session_id = 0;

// BLE Server (Node A)
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

// BLE Client (Node B)
BLEClient*  pClient  = NULL;
BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
bool doConnect = false;
bool connected = false;

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc = (crc >> 1);
        }
    }
    return crc;
}

// ------------------------------------------------------------------
// NODE B: CLIENT RX HANDLER
// ------------------------------------------------------------------
uint32_t t_rx_start = 0;
int packets_got = 0;

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length != sizeof(FrameHeader) + BLOCK_SIZE + sizeof(FrameTrailer)) return; // Bad frame size

    if (packets_got == 0) {
        t_rx_start = millis();
        memset(packet_received, 0, sizeof(packet_received));
        Serial.println("\n[BLE RX] Stream started...");
    }

    FrameHeader header;
    memcpy(&header, pData, sizeof(FrameHeader));
    
    if (header.magic[0] != 0xAA || header.magic[1] != 0x55) return;

    FrameTrailer trailer;
    memcpy(&trailer, pData + sizeof(FrameHeader) + BLOCK_SIZE, sizeof(FrameTrailer));

    uint16_t calc_crc = crc16(pData, sizeof(FrameHeader) + BLOCK_SIZE);
    
    if (calc_crc == trailer.crc) {
        int16_t session_diff = (int16_t)(header.session_id - current_session_id);
        if (session_diff >= 0) {
            current_session_id = header.session_id;
            uint8_t seq = header.seq_id;
            
            if (seq <= DATA_BLOCKS && !packet_received[seq]) {
                packet_received[seq] = true;
                if (seq < DATA_BLOCKS) memcpy(recovered_data[seq], pData + sizeof(FrameHeader), BLOCK_SIZE);
                else memcpy(received_parity, pData + sizeof(FrameHeader), BLOCK_SIZE);
                packets_got++;
            }
        }
    }

    if (packets_got >= DATA_BLOCKS + 1) {
        Serial.printf("[BLE RX] Stream complete. Received %d frames in %d ms.\n", packets_got, millis() - t_rx_start);
        
        // FEC Recovery & Decode
        int missing_count = 0;
        int first_missing_idx = -1;
        for(int i=0; i<=DATA_BLOCKS; i++) {
            if(!packet_received[i]) {
                missing_count++;
                if (first_missing_idx == -1) first_missing_idx = i;
            }
        }
        
        if (missing_count == 1 && first_missing_idx != DATA_BLOCKS) {
            Serial.printf("  -> [FEC] Missing frame %d. Engaging XOR recovery...\n", first_missing_idx);
            LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::recover_missing(recovered_data, received_parity, first_missing_idx);
        } else if (missing_count > 1) {
            Serial.printf("  -> [FEC] Collapse! Lost %d frames.\n", missing_count);
            packets_got = 0;
            return;
        }

        for(int i=0; i<DATA_BLOCKS; i++) {
            memcpy(reconstructed_payload + (i * BLOCK_SIZE), recovered_data[i], BLOCK_SIZE);
        }
        
        static IBLT<200, 3> incoming_iblt;
        memcpy(&incoming_iblt.cells, reconstructed_payload, sizeof(incoming_iblt.cells));

        static IBLT<200, 3> delta;
        incoming_iblt.subtract(localNode.iblt, delta);

        size_t added_count = 0, removed_count = 0, iterations = 0;
        uint32_t t_decode = millis();
        bool success = delta.decode(added_buf, 200, &added_count, removed_buf, 200, &removed_count, &iterations);
        
        Serial.printf("Decode Result     : %s (Time: %d ms)\n", success ? "PASS" : "FAIL", millis() - t_decode);
        packets_got = 0; // Reset for next sync
    }
}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) { connected = true; Serial.println("[BLE] Connected to Node A."); }
    void onDisconnect(BLEClient* pclient) { connected = false; Serial.println("[BLE] Disconnected."); }
};

bool connectToServer() {
    Serial.println("[BLE] Forming a connection to Node A...");
    pClient  = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    
    // We request MTU of 512 to easily fit 127 byte chunks
    BLEDevice::setMTU(512);

    // This address must match Node A's MAC. For simplicity, we assume Node B knows it or scans for it.
    // In a real scenario, we use BLE Scan. Let's do a hardcoded approach or rely on pairing.
    // Since we don't have the MAC, we will just scan for the service UUID.
    return false; // See loop() for scan logic
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            pClient = BLEDevice::createClient();
            pClient->setClientCallbacks(new MyClientCallback());
            pClient->connect(&advertisedDevice);
            
            BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
            if (pRemoteService != nullptr) {
                pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
                if(pRemoteCharacteristic != nullptr) {
                    if(pRemoteCharacteristic->canNotify()) pRemoteCharacteristic->registerForNotify(notifyCallback);
                    doConnect = true;
                }
            }
        }
    }
};

// ------------------------------------------------------------------
// NODE A: SERVER TX HANDLER
// ------------------------------------------------------------------
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("[BLE] Client Connected."); };
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; Serial.println("[BLE] Client Disconnected."); }
};

void tx_framed_iblt() {
    if (!deviceConnected) {
        Serial.println("[TX] Cannot send, no BLE client connected.");
        return;
    }
    
    current_session_id++;
    Serial.printf("\n[TX] Initiating Session %d over BLE...\n", current_session_id);
    
    memcpy(raw_payload, &localNode.iblt.cells, sizeof(localNode.iblt.cells));
    for(int i=0; i<DATA_BLOCKS; i++) {
        memcpy(data_blocks[i], raw_payload + (i * BLOCK_SIZE), BLOCK_SIZE);
    }
    LithoMeshXORErasure<DATA_BLOCKS, BLOCK_SIZE>::generate_parity(data_blocks, parity_block);

    uint8_t frame_buf[sizeof(FrameHeader) + BLOCK_SIZE + sizeof(FrameTrailer)];
    
    uint32_t t_tx_start = millis();
    for(int i=0; i<=DATA_BLOCKS; i++) {
        FrameHeader header = {{0xAA, 0x55}, current_session_id, (uint8_t)i};
        memcpy(frame_buf, &header, sizeof(FrameHeader));
        
        if (i < DATA_BLOCKS) memcpy(frame_buf + sizeof(FrameHeader), data_blocks[i], BLOCK_SIZE);
        else memcpy(frame_buf + sizeof(FrameHeader), parity_block, BLOCK_SIZE);
        
        uint16_t crc = crc16(frame_buf, sizeof(FrameHeader) + BLOCK_SIZE);
        FrameTrailer trailer = {crc};
        memcpy(frame_buf + sizeof(FrameHeader) + BLOCK_SIZE, &trailer, sizeof(FrameTrailer));
        
        // Push notification
        pCharacteristic->setValue(frame_buf, sizeof(frame_buf));
        pCharacteristic->notify();
        delay(10); // BLE stack backpressure relief
    }
    Serial.printf("[TX] Sent %d frames in %d ms.\n", DATA_BLOCKS + 1, millis() - t_tx_start);
}

void setup() {
    Serial.begin(115200);
    while (!Serial); delay(2000);

    Serial.println("\n\n============================================");
    Serial.println(" LithoMesh Level 9: Milestone Alpha (BLE) ");
    Serial.println("============================================");

    for(uint32_t i=1; i<=1000; i++) localNode.log_event(i);

    BLEDevice::init("LithoMesh_Node");

    if (ROLE_NODE_A) {
        // Apply Divergence
        for(uint32_t i=1001; i<=1050; i++) localNode.log_event(i);
        Serial.println("[STATE] Divergence Applied: 50 missing events.");
        
        pServer = BLEServer::create();
        pServer->setCallbacks(new MyServerCallbacks());
        BLEService *pService = pServer->createService(SERVICE_UUID);
        pCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_NOTIFY
                          );
        pCharacteristic->addDescriptor(new BLE2902());
        pService->start();
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pServer->getAdvertising()->start();
        Serial.println("[BLE] Advertising started. Type 'SYNC' to blast state once connected.");
    } else {
        Serial.println("[BLE] Scanning for Node A...");
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setActiveScan(true);
        pBLEScan->start(5, false);
    }
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "SYNC" && ROLE_NODE_A) {
            tx_framed_iblt();
        }
    }
    
    if (!ROLE_NODE_A && !doConnect) {
        // Rescan periodically if dropped
        delay(2000);
    }
}
