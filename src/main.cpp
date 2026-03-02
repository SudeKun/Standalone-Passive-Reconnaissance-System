#include <Arduino.h>

// =========================================================================
//                       ESP-201 (ESP8266) CODE
// =========================================================================
#if defined(ESP8266)

#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include "user_interface.h"
}

int channel = 1;

// --- TOPOLOGY FUNCTION ---
void topology(uint8_t* frame, int8_t rssi) {
  // Filter only data frames (Type 0x02, Subtype 0x08 is typical for Data)
  if ((frame[0] & 0x0F) == 0x08) {
    
    uint8_t* addr1 = frame + 4;  // Address 1
    uint8_t* addr2 = frame + 10; // Address 2
    uint8_t* addr3 = frame + 16; // Address 3 (Final Destination or Original Source)
    
    // To DS and From DS bits
    bool to_ds   = frame[1] & 0x01;       
    bool from_ds = (frame[1] & 0x02) >> 1; 

    uint8_t* ap_mac = nullptr;
    uint8_t* client_mac = nullptr;
    uint8_t* gateway_mac = addr3; // Define Address 3 as the gateway/remote target
    const char* direction = "";

    if (to_ds && !from_ds) {
      // Upload (Device -> AP -> Gateway)
      ap_mac = addr1;     
      client_mac = addr2; 
      direction = "Upload";
    } 
    else if (!to_ds && from_ds) {
      // Download (Gateway -> AP -> Device)
      client_mac = addr1; 
      ap_mac = addr2;     
      direction = "Download";
    }

    // Proceed if valid connection found and addresses are not Broadcast (FF)
    if (ap_mac != nullptr && client_mac != nullptr && client_mac[0] != 0xFF && ap_mac[0] != 0xFF) {
        
      // Check if addr3 (gateway_mac) is distinct from both ap_mac and client_mac
      // Also ensure gateway_mac is not a broadcast address
      bool is_gateway_different = (memcmp(gateway_mac, ap_mac, 6) != 0) && 
                                  (memcmp(gateway_mac, client_mac, 6) != 0) && 
                                  (gateway_mac[0] != 0xFF);

      if (is_gateway_different) {
          // If addr3 is distinct, print Extended Topology (3-Node)
          if (to_ds && !from_ds) {
              // Upload Path
              Serial.printf("TOPOLOGY [3-Point] | RSSI: %d | Device: %02x:%02x:%02x:%02x:%02x:%02x -> Access Point: %02x:%02x:%02x:%02x:%02x:%02x -> Gateway: %02x:%02x:%02x:%02x:%02x:%02x\n",
                             rssi,
                             client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5],
                             ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5],
                             gateway_mac[0], gateway_mac[1], gateway_mac[2], gateway_mac[3], gateway_mac[4], gateway_mac[5]);
          } else {
              // Download Path (Directional arrows reversed)
              Serial.printf("TOPOLOGY [3-Point] | RSSI: %d | Gateway: %02x:%02x:%02x:%02x:%02x:%02x -> Access Point: %02x:%02x:%02x:%02x:%02x:%02x -> Device: %02x:%02x:%02x:%02x:%02x:%02x\n",
                             rssi,
                             gateway_mac[0], gateway_mac[1], gateway_mac[2], gateway_mac[3], gateway_mac[4], gateway_mac[5],
                             ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5],
                             client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5]);
          }
      } else {
          // If addr3 is not distinct or invalid, output standard 2-node topology
          Serial.printf("TOPOLOGY [%s] | RSSI: %d | AP/Modem: %02x:%02x:%02x:%02x:%02x:%02x <-> Device: %02x:%02x:%02x:%02x:%02x:%02x\n",
                        direction, rssi,
                        ap_mac[0], ap_mac[1], ap_mac[2], ap_mac[3], ap_mac[4], ap_mac[5],
                        client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5]);
      }
    }
  }
}

// --- SINFFER FUNCTION ---
void sniffer_callback(uint8_t *buf, uint16_t len) {
  if (len < 60) return; 

  int8_t rssi = (int8_t)buf[0]; 
  uint8_t* frame = buf + 12; // Skip the ESP8266 management header

  topology(frame, rssi); // Call topology function for DATA frames

  // 1. Extract MAC and Sequence Info
  uint8_t* mac = frame + 10; // Source MAC is at offset 10
  
  // Sequence number is 12 bits starting at byte 22
  uint16_t seq_control = frame[22] | (frame[23] << 8);
  uint16_t seq_num = (seq_control >> 4); 

  // 2. Check "Locally Administered Bit" (Fake MAC Check)
  // If the second hex digit is 2, 6, A, or E, it's likely a spoofed/randomized MAC
  bool is_local = mac[0] & 0b00000010;

  // 3. Process Beacon Frames (APs)
  if (frame[0] == 0x80) {
    uint8_t tagLength = frame[37];
    if (tagLength > 0 && tagLength <= 32) {
      char ssid[33] = {0};
      memcpy(ssid, frame + 38, tagLength);
      
      // Added SEQ and LOCAL tags to the output string
      Serial.printf("RSSI: %d | MAC: %02x:%02x:%02x:%02x:%02x:%02x | SEQ: %d | LOCAL: %d | SSID: %s\n",
                    rssi, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], seq_num, is_local, ssid);
    }
  }
  
  // 4. Process Probe Requests (Devices)
  else if (frame[0] == 0x40) {
    Serial.printf("RSSI: %d | MAC: %02x:%02x:%02x:%02x:%02x:%02x | SEQ: %d | LOCAL: %d | Type: Probe Request\n",
                  rssi, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], seq_num, is_local);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give the serial port a second to stabilize after the boot gibberish
  
  // This tells us the code actually started running!
  Serial.println("ESP-201 SNIFFER ONLINE AND SCANNING!");

  // Initialize the Wi-Fi Sniffer
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  wifi_promiscuous_enable(1);
}

void loop() {
  // Rapidly sweep through all 13 Wi-Fi channels to catch everything
  channel = (channel % 13) + 1;
  wifi_set_channel(channel);
  delay(200); 
}

// =========================================================================
//                       ESP32-WROOM-32U CODE
// =========================================================================
#elif defined(ESP32)

#include <HardwareSerial.h>
#include <NimBLEDevice.h>
#include <map>
#include <string> 

HardwareSerial ESP201Serial(2); 

// --- THE ASSET LEDGER ---
// Key: MAC Address | Value: Connection Type (SSID, Probe Request, or BLE)
std::map<std::string, std::string> knownAssets;

// BLUETOOTH CALLBACK 
class MyBLECallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        std::string macAddr = advertisedDevice->getAddress().toString();
        std::string connType = "BLE"; // The type for all Bluetooth devices
        
        // Check: Is it brand new? OR did its connection type change?
        if (knownAssets.find(macAddr) == knownAssets.end() || knownAssets[macAddr] != connType) {
            knownAssets[macAddr] = connType; // Save the new type
            
            int rssi = advertisedDevice->getRSSI();
            Serial.printf("[BLE] Local  | RSSI: %d | MAC: %s\n", rssi, macAddr.c_str());
        }
    }
};

void setup() {
  Serial.begin(115200); 
  ESP201Serial.begin(115200, SERIAL_8N1, 16, 17); 

  Serial.println("Starting Dual-Radio Master System...");

  NimBLEDevice::init("");
  NimBLEScan* pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyBLECallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(0, nullptr, false); 
}

void loop() {
  if (ESP201Serial.available()) {
    String wifiData = ESP201Serial.readStringUntil('\n');
    wifiData.trim(); 
    
    int macIndex = wifiData.indexOf("MAC: ");
    
    if (macIndex != -1) {
      String macSub = wifiData.substring(macIndex + 5, macIndex + 22);
      std::string macAddr = macSub.c_str();
      
      std::string connType = "Unknown";
      if (wifiData.length() > (unsigned int)(macIndex + 25)) {
         connType = wifiData.substring(macIndex + 25).c_str();
      }
      
      // RULE 1: Is this MAC address completely brand new?
      if (knownAssets.find(macAddr) == knownAssets.end()) {
          knownAssets[macAddr] = connType; // Save it
          Serial.print("[WIFI] ESP-201 | ");
          Serial.println(wifiData);
      } 
      // RULE 2: We know this MAC. Did it upgrade from a Probe to an SSID?
      else {
          std::string oldType = knownAssets[macAddr];
          
          // If we previously only knew it was a Probe, but NOW it reveals an SSID -> Upgrade and Print!
          if (oldType.find("Type: Probe") != std::string::npos && connType.find("SSID:") != std::string::npos) {
              knownAssets[macAddr] = connType; // Upgrade the ledger
              Serial.print("[WIFI] ESP-201 | ");
              Serial.println(wifiData);
          }
      }
      // If it already has an SSID and sends a Probe Request, it will be silently ignored!
      
    } else if (wifiData.length() > 0) {
        Serial.print("[SYS]  ");
        Serial.println(wifiData);
    }
  }
}

#endif