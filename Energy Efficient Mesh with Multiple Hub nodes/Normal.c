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
const int deviceNumber = 1; // adjust for each node

// Local state variables
uint8_t myHopCount = 255;
uint32_t lastSeqNum = 0;
uint32_t myHubId = 0;  // Set this when you receive HUB_ID
std::set<uint32_t> directNeighbors;
unsigned long lastUpdateHopTime = 0;
const unsigned long updateHopTimeout = 60000; // 60 seconds

void sendToAllNeighbors(String &msg, uint32_t excludeNode) {
  // Retrieve the list of currently connected nodes (neighbors)
  
  // Log the outgoing message
  Serial.print("[SEND] Message: ");
  Serial.println(msg);
  
  // Iterate through the neighbor list and send the message to each neighbor individually
  for (uint32_t node : directNeighbors) {
    if(node!=myHubId && node!= excludeNode) {
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

void HopCountUpdated(int receivedHop, uint32_t excludeNode){
    // Update the hop count and broadcast the message to neighbors
    myHopCount = receivedHop + 1;
    String broadcastMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum) + ":" + String(myHubId);
    sendToAllNeighbors(broadcastMsg,excludeNode);
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
        String updateMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum) + ":" + String(myHubId);
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
  // If the message is a hop count update from a neighbor:
  // Expected format: "UPDATE_HOP:<hopCount>"
    if (msg.startsWith("UPDATE_HOP:")) {

        int firstColon = msg.indexOf(':');
        int secondColon = msg.indexOf(':', firstColon + 1);
        int thirdColon = msg.indexOf(':', secondColon + 1);

        int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
        uint32_t receivedSeq = msg.substring(secondColon + 1, thirdColon).toInt();
        uint32_t incomingHubId = msg.substring(thirdColon + 1).toInt();

        if (receivedHop + 1 < myHopCount) {
            //Inform old hub before switching
            if (myHubId != 0 && myHubId != incomingHubId) {
                String leaveMsg = "LEAVE:" + String(mesh.getNodeId());
                mesh.sendSingle(myHubId, leaveMsg);
                Serial.printf("[NODE] Sent LEAVE to old hub %u\n", myHubId);
            }
            myHopCount = receivedHop + 1;
            myHubId = incomingHubId;
            lastSeqNum = receivedSeq;
            lastUpdateHopTime = millis();
            HopCountUpdated(receivedHop, from);
            Serial.printf("[NODE] Switched to Hub %u with better hop\n", myHubId);
        }


        else if (incomingHubId == myHubId) {
            if (isNewer(receivedSeq, lastSeqNum)) {
                lastSeqNum = receivedSeq;
                lastUpdateHopTime = millis();
                HopCountUpdated(receivedHop, from);
                Serial.printf("[NODE] Seq update from same Hub %u: Seq %u\n", myHubId, lastSeqNum);
            }
        }

    // 3. If it's from a different hub, ignore it completely
        else if (incomingHubId != myHubId) {
            Serial.printf("[NODE] Ignoring message from different hub %u\n", incomingHubId);
            return;
        }
    }

    else if (msg.startsWith("REQUEST:")) {
        uint32_t requestingHubId = msg.substring(8).toInt();  // Extract hub ID from message

        int sensorVal = 18;  // Simulated reading
        String sensorMsg = "DATA:" + deviceType + "-" + String(deviceNumber) +
            ":Sensor=" + String(sensorVal) +
            ":Hop=" + String(myHopCount) +
            ":Sequence=" + String(lastSeqNum) +
            ":NodeId=" + String(mesh.getNodeId()) +
            ":Time=" + String(millis());

        mesh.sendSingle(requestingHubId, sensorMsg);
        Serial.printf("[NODE] Sent sensor data to requesting hub %u\n", requestingHubId);
    }
}

void setup() {  
  Serial.begin(115200);
  mesh.setDebugMsgTypes(STARTUP); // only minimal debug info
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
}

void loop() {
  mesh.update();


  if (millis() - lastUpdateHopTime > updateHopTimeout) {
    Serial.println("[NODE] No UPDATE_HOP received in 60 seconds. Resetting hop count and sequence.");
    // Reset state values
    myHubId = 0; 
    myHopCount = 255;
    lastSeqNum = 0;
    // Reset the timer to avoid repeated resets
    lastUpdateHopTime = millis();
  }
}
