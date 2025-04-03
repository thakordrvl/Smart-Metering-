#include "painlessMesh.h"

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MAX_SEQ 1000
// #define DEVICE_NUMBER 2  // Change this per device



Scheduler userScheduler;
painlessMesh mesh;

// Device configuration (set before upload)
const String deviceType = "ESP32";
const int deviceNumber = 1; // adjust for each node

// Local state variables
uint8_t myHopCount = 255;
uint32_t lastSeqNum = 0;
uint32_t myHubId = 0;  // Set this when you receive HUB_ID

bool isNewer(uint16_t newSeq, uint16_t lastSeq) {
  if (lastSeq == 0)
    return true; // Accept the first sequence number.
  
  // If there's no wrap-around, newSeq is newer.
  if(newSeq > lastSeq)
    return true;

  if(lastSeq==1000 && newSeq>=1 && newSeq<=995)
    return true;
    
  // Otherwise, calculate the difference using modulo arithmetic.
  return false;
}

void HopCountUpdated(int receivedHop){
  myHopCount = receivedHop + 1;
  String broadcastMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum);
  mesh.sendBroadcast(broadcastMsg);
  Serial.printf("[NODE] Updated hop count to %d, broadcasting: %s\n", myHopCount, broadcastMsg.c_str());
  
  if (myHubId != 0) {
    String hubUpdateMsg = "UPDATE_HOP_HUB:" + String(myHopCount) + ":" + String(mesh.getNodeId());
    mesh.sendSingle(myHubId, hubUpdateMsg);
    Serial.printf("[NODE] Sent hub-specific update: %s\n", hubUpdateMsg.c_str());
  }
}

// When a new node connects to this node, send it the hub id and the current hop count
void newConnectionCallback(uint32_t nodeId) {
  // Send hub id message using the current node's ID (assuming the hub's own ID is the one running this code)
  String hubMsg = "HUB_ID:" + String(myHubId);
  mesh.sendSingle(nodeId, hubMsg);
  
  // Then, send the current hop count message with a colon separator
  String updateMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum);
  mesh.sendSingle(nodeId, updateMsg);
}

// When a message is received
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[NODE] Received from %u: %s\n", from, msg.c_str());
  Serial.printf("[NODE] My Node ID: %u\n", mesh.getNodeId());
  
  // If the message is a hub id update:
  // Expected format: "HUB_ID:<hubId>"
  if (msg.startsWith("HUB_ID:")) {
    uint32_t newHubId = msg.substring(7).toInt();
    if (newHubId != myHubId) {
      myHubId = newHubId;
      String hubMsg = "HUB_ID:" + String(myHubId);
      mesh.sendBroadcast(hubMsg);
      // Serial.printf("[NODE] Updated Hub ID to %u, broadcasting: %s\n", myHubId, hubMsg.c_str());
    }
  }
  // If the message is a hop count update from a neighbor:
  // Expected format: "UPDATE_HOP:<hopCount>"
  else if (msg.startsWith("UPDATE_HOP:")) {
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    if (secondColon != -1) {
      int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
      uint32_t receivedSeq = msg.substring(secondColon + 1).toInt();
      
      // Use isNewer() to determine if this update should be processed.
      if (isNewer(receivedSeq, lastSeqNum)) {
        lastSeqNum = receivedSeq;
        HopCountUpdated(receivedHop);
      }

      else if(receivedHop + 1 < myHopCount) {
        HopCountUpdated(receivedHop);
      }
    }
  }

  else if (msg.startsWith("REQUEST")) {
    int sensorVal = 18;
    String sensorMsg = "DATA:" + deviceType + "-" + String(deviceNumber) +
    ":Sensor=" + String(sensorVal) +
    ":Hop=" + String(myHopCount) +
    ":Sequence=" + String(lastSeqNum) +
    ":NodeId=" + String(mesh.getNodeId()) +
    ":Time=" + String(millis());
    if (myHubId != 0) {
      mesh.sendSingle(myHubId, sensorMsg);
      Serial.println("[NODE] Sent sensor data on REQUEST_SEND");
    } else {
      Serial.println("[NODE] Hub ID not set, cannot respond to REQUEST_SEND");
    }
  } 
}

void setup() {  
  Serial.begin(115200);
  mesh.setDebugMsgTypes(STARTUP); // only minimal debug info
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.setContainsRoot(true);

  // Set up received message callback
  mesh.onReceive(&receivedCallback);
  // Set up new connection callback to send hub and hop count messages individually
  mesh.onNewConnection(&newConnectionCallback);

}

void loop() {
  mesh.update();
}
