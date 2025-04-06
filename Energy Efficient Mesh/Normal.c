#include "painlessMesh.h"
#include <set>

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MAX_SEQ 1000

Scheduler userScheduler;
painlessMesh mesh;

// Device configuration (set before upload)
const String deviceType = "ESP32";
const int deviceNumber = 1 // adjust for each node

// Local state variables
uint8_t myHopCount = 255;
uint32_t lastSeqNum = 0;
uint32_t myHubId = 0;  // Set this when you receive HUB_ID
std::set<uint32_t> directNeighbors;


void sendToAllNeighbors(String &msg) {
  // Retrieve the list of currently connected nodes (neighbors)
  
  // Log the outgoing message
  Serial.print("[SEND] Message: ");
  Serial.println(msg);
  
  // Iterate through the neighbor list and send the message to each neighbor individually
  for (uint32_t node : directNeighbors) {
    if(node!=myHubId){
      Serial.print("[SEND] Sending to node: ");
      Serial.println(node);
      mesh.sendSingle(node, msg);
    }
  }
}


bool isNewer(uint16_t newSeq, uint16_t lastSeq) {

  if(newSeq>lastSeq)
    return true;

  else if(newSeq==lastSeq)
    return false;

  const uint16_t HALF_MAX_SEQ = MAX_SEQ / 2;
  return ((newSeq > lastSeq) && (newSeq - lastSeq < HALF_MAX_SEQ)) ||
         ((lastSeq > newSeq) && (lastSeq - newSeq > HALF_MAX_SEQ));
}

void HopCountUpdated(int receivedHop){
  myHopCount = receivedHop + 1;
  String broadcastMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum);
  sendToAllNeighbors(broadcastMsg);
  Serial.printf("[NODE] Updated hop count to %d, broadcasting: %s\n", myHopCount, broadcastMsg.c_str());
  
  if (myHubId != 0) {
    String hubUpdateMsg = "UPDATE_HOP_HUB:" + String(myHopCount) + ":" + String(mesh.getNodeId());
    mesh.sendSingle(myHubId, hubUpdateMsg);
    Serial.printf("[NODE] Sent hub-specific update: %s\n", hubUpdateMsg.c_str());
  }
}

// When a new node connects to this node, send it the hub id and the current hop count
void newConnectionCallback(uint32_t nodeId) {
  // Always add the node to direct neighbors
  directNeighbors.insert(nodeId);
  Serial.printf("[NODE] New connection from node %u\n", nodeId);

  // Only send hub-specific messages if hub info is available.
  if (myHubId != 0 && lastSeqNum != 0 && nodeId != myHubId) {
    String hubMsg = "HUB_ID:" + String(myHubId);
    mesh.sendSingle(nodeId, hubMsg);
    Serial.printf("[NODE] Sent HUB_ID message: %s\n", hubMsg.c_str());
  
    String updateMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum);
    mesh.sendSingle(nodeId, updateMsg);
    Serial.printf("[NODE] Sent update message: %s\n", updateMsg.c_str());
  }
}

void droppedConnectionCallback(uint32_t nodeId) {
  Serial.printf("[NODE] Connection dropped from node %u\n", nodeId);
  directNeighbors.erase(nodeId);
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
      sendToAllNeighbors(hubMsg);
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

      else if(receivedHop + 1 < myHopCount && (receivedSeq == lastSeqNum)) {
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
  
  else if (msg.startsWith("FRESH:")) {
    Serial.println("[NODE] FRESH message received. Resetting state...");
    // Optionally parse the hub ID from the message if desired:
    myHubId = msg.substring(6).toInt();
    // Reset the hop count and sequence number to their defaults
    myHopCount = 255;
    lastSeqNum = 0;
    // Optionally, clear direct neighbors if you want to force re-discover
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
  mesh.onDroppedConnection(&droppedConnectionCallback);
}

void loop() {
  mesh.update();
}
