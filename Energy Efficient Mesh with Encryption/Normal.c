#include "painlessMesh.h"
#include <set>
#include <AESLib.h>
#include <base64.h>

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

Scheduler userScheduler;
painlessMesh mesh;

// Device configuration
const String deviceType = "ESP32";  // or "ESP8266"
const int    deviceNumber = 1;

// Mesh state variables
uint8_t myHopCount = 255;
uint32_t lastSeqNum = 0, myHubId = 0;
std::set<uint32_t> directNeighbors;
unsigned long lastUpdateHopTime = 0;
const unsigned long updateHopTimeout = 60000;

// AES key - 16 bytes (128-bit)
const byte AES_KEY[16] = { 0x8B, 0x18, 0x45, 0x30, 0x87, 0xF8, 0x93, 0x14, 0x62, 0xF6, 0x36, 0xEA, 0x5D, 0x61, 0x06, 0x81 };
const byte AES_IV[16] = { 0x0F, 0xA4, 0x01, 0x04, 0x13, 0x82, 0xA6, 0x94, 0x81, 0x08, 0x39, 0x96, 0xFE, 0x13, 0xF2, 0x5B };

AESLib aesLib;

// Function for base64 encoding
String base64Encode(const byte* data, size_t length) {
  static const char* ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t outLen = 4 * ((length + 2) / 3);
  String result;
  result.reserve(outLen);
  
  for (size_t i = 0; i < length; i += 3) {
    uint32_t b = (data[i] << 16);
    if (i + 1 < length) b |= (data[i + 1] << 8);
    if (i + 2 < length) b |= data[i + 2];
    
    result += ALPHABET[(b >> 18) & 0x3F];
    result += ALPHABET[(b >> 12) & 0x3F];
    result += (i + 1 < length) ? ALPHABET[(b >> 6) & 0x3F] : '=';
    result += (i + 2 < length) ? ALPHABET[b & 0x3F] : '=';
  }
  
  return result;
}

String encryptMessage(const String &plaintext) {
  // Convert String to byte array
  int inputLength = plaintext.length();
  byte input[inputLength + 16]; // Extra space for padding
  memcpy(input, plaintext.c_str(), inputLength);
  
  // Calculate padded length (must be multiple of 16)
  int paddedLength = (inputLength + 15) & ~15;
  
  // Apply PKCS#7 padding
  byte padValue = paddedLength - inputLength;
  for (int i = inputLength; i < paddedLength; i++) {
    input[i] = padValue;
  }
  
  // Create output buffer (same size as padded input)
  byte output[paddedLength];
  byte iv_copy[16];
  memcpy(iv_copy, AES_IV, 16);
  
  // Encrypt using the correct API for your AESLib version
  uint16_t encLength = aesLib.encrypt(input, paddedLength, output, AES_KEY, 128, iv_copy);
  
  // Base64 encode using our custom function
  return base64Encode(output, encLength);
}

// Rest of your mesh network code remains the same...


void sendToAllNeighbors(const String &msg, uint32_t excludeNode) {
  Serial.print("[SEND] "); Serial.println(msg);
  for (uint32_t node : directNeighbors) {
    if (node != myHubId && node != excludeNode) {
      mesh.sendSingle(node, msg);
    }
  }
}

bool isNewer(uint16_t newSeq, uint16_t lastSeq) {
  if (newSeq > lastSeq) return true;
  if (newSeq == lastSeq) return false;
  const uint16_t HALF = 1000/2;
  return ((newSeq > lastSeq) && (newSeq - lastSeq < HALF)) ||
         ((lastSeq > newSeq) && (lastSeq - newSeq > HALF));
}

void HopCountUpdated(int hop, uint32_t fromNode) {
  myHopCount = hop + 1;
  String bc = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum);
  sendToAllNeighbors(bc, fromNode);
  if (myHubId) {
    String hubMsg = "UPDATE_HOP_HUB:" + String(myHopCount) + ":" +
                    String(mesh.getNodeId()) + ":" + String(lastSeqNum);
    mesh.sendSingle(myHubId, hubMsg);
  }
}

void newConnectionCallback(uint32_t nodeId) {
  directNeighbors.insert(nodeId);
  if (myHubId && lastSeqNum) {
    mesh.sendSingle(nodeId, "HUB_ID:" + String(myHubId));
    mesh.sendSingle(nodeId, "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum));
  }
}

void droppedConnectionCallback(uint32_t nodeId) {
  directNeighbors.erase(nodeId);
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.print("[RECV] "); Serial.println(msg);

  if (msg.startsWith("HUB_ID:")) {
    uint32_t h = msg.substring(7).toInt();
    if (h != myHubId) {
      myHubId = h;
      sendToAllNeighbors("HUB_ID:" + String(myHubId), from);
    }
  }
  else if (msg.startsWith("UPDATE_HOP:")) {
    lastUpdateHopTime = millis();
    int c1 = msg.indexOf(':'), c2 = msg.indexOf(':', c1+1);
    int hop = msg.substring(c1+1, c2).toInt();
    uint32_t seq = msg.substring(c2+1).toInt();
    if (isNewer(seq, lastSeqNum)) {
      lastSeqNum = seq;
      HopCountUpdated(hop, from);
    }
  }
  else if (msg.startsWith("REQUEST")) {
    int sensorVal = 18;  // replace with actual sensor read
    String prefix = deviceType + "-" + String(deviceNumber);
    String plain  = "Sensor=" + String(sensorVal) +
                    ":Hop="    + String(myHopCount) +
                    ":Seq="    + String(lastSeqNum) +
                    ":Node="   + String(mesh.getNodeId()) +
                    ":Time="   + String(millis());
    String encrypted = encryptMessage(plain);
    String out = "DATA:" + prefix + ":" + encrypted;
    if (myHubId) {
      mesh.sendSingle(myHubId, out);
      Serial.println("[NODE] Sent encrypted data");
    }
    else {
      Serial.println("[NODE] Hub ID not set, cannot respond to REQUEST_SEND");
    }
  }
  else if (msg.startsWith("FRESH:")) {
    myHubId = msg.substring(6).toInt();
    myHopCount = 255;
    lastSeqNum = 0;
  }
}

void setup() {
  Serial.begin(115200);
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.setContainsRoot(true);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
}

void loop() {
  mesh.update();
  if (millis() - lastUpdateHopTime > updateHopTimeout) {
    myHopCount = 255;
    lastSeqNum = 0;
    lastUpdateHopTime = millis();
  }
}
