#include "painlessMesh.h"
#include <set>

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MAX_SEQ 1000

Scheduler userScheduler;
painlessMesh mesh;

// Device configuration (adjust per node)
const String deviceType = "ESP8266";
const int deviceNumber = 2;

// State variables
uint8_t myHopCount = 255;            // Default: unreachable
uint32_t lastSeqNum = 0;             // Sequence number from hub
uint32_t myHubId = 0;                // ID of the currently assigned hub
uint8_t mylocalHubId = 0;  // Unique ID per hub (manually assigned)
std::set<uint32_t> directNeighbors;  // Connected neighbors
unsigned long lastUpdateHopTime = 0;
const unsigned long updateHopTimeout = 60000; // Reset after 60s of silence


bool sendFromNormal(uint32_t targetId, const String& msg) {
  bool sent = mesh.sendSingle(targetId, msg);
  Serial.println("[NODE-%s-%d] Sent to %u? %s | Message: %s\n", deviceType.c_str(), deviceNumber, targetId, sent ? "Yes" : "No", msg.c_str());

  if (!sent) {
    auto list = mesh.getNodeList(true);
    Serial.print("Known nodes: ");
    for (auto n : list) Serial.print(n), Serial.print(" ");
    Serial.println();

    Serial.println("[NODE-%s-%d] [MESSAGE FAILED] %s\n", deviceType.c_str(), deviceNumber, msg.c_str());
    if (std::find(list.begin(), list.end(), targetId) == list.end()) {
      Serial.println("[NODE-%s-%d] [ERROR] Target %u not found in routing table!\n", deviceType.c_str(), deviceNumber, targetId);
    } else {
      Serial.println("[NODE-%s-%d] [WARN] Target %u is known but message failed to send.\n", deviceType.c_str(), deviceNumber, targetId);
    }
  }

  return sent;
}

// Send a message to all direct neighbors except excluded nodes (e.g. hub)
void sendToAllNeighbors(String &msg, uint32_t excludeNode) {
  Serial.println("[NODE-%s-%d] [SEND] Message: %s\n", deviceType.c_str(), deviceNumber, msg.c_str());

  for (uint32_t node : directNeighbors) {
    if (node != myHubId && node != excludeNode) {
      Serial.println("[NODE-%s-%d] [SEND] Sending to node: %u\n", deviceType.c_str(), deviceNumber, node);
      sendFromNormal(node, msg);
    }
  }
}

// Wrap-around sequence number comparison
bool isNewer(uint16_t newSeq, uint16_t lastSeq) {
  if (newSeq > lastSeq) return true;
  if (newSeq == lastSeq) return false;

  const uint16_t HALF_MAX_SEQ = MAX_SEQ / 2;
  return ((newSeq > lastSeq) && (newSeq - lastSeq < HALF_MAX_SEQ)) ||
         ((lastSeq > newSeq) && (lastSeq - newSeq > HALF_MAX_SEQ));
}

// Called when hop count is updated — rebroadcasts update
void HopCountUpdated(int receivedHop, uint32_t excludeNode){
  myHopCount = receivedHop + 1;

  // Broadcast updated hop and sequence info to neighbors
  String broadcastMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum) + ":" + String(myHubId) + ":" + String(mylocalHubId);
  sendToAllNeighbors(broadcastMsg, excludeNode);
  Serial.println("[NODE-%s-%d] Updated hop count to %d, broadcasting: %s\n", deviceType.c_str(), deviceNumber, myHopCount, broadcastMsg.c_str());

  // Inform hub directly as well
  if (myHubId != 0) {
    String hubUpdateMsg = "UPDATE_HOP_HUB:" + String(myHopCount) + ":" + String(mesh.getNodeId()) ;
    sendFromNormal(myHubId, hubUpdateMsg);
  }
}

// Called when a new neighbor connects
void newConnectionCallback(uint32_t nodeId) {
  directNeighbors.insert(nodeId);
  Serial.println("[NODE-%s-%d] New connection from node %u\n", deviceType.c_str(), deviceNumber, nodeId);

  // Send hop and sequence info if available and not to the hub
  if (myHubId != 0 && lastSeqNum != 0 && nodeId != myHubId) {
    String updateMsg = "UPDATE_HOP:" + String(myHopCount) + ":" + String(lastSeqNum) + ":" + String(myHubId) + ":" + String(mylocalHubId);
    sendFromNormal(nodeId, updateMsg);
  }
}

// Called when a neighbor disconnects
void droppedConnectionCallback(uint32_t nodeId) {
  Serial.println("[NODE-%s-%d] Connection dropped from node %u\n", deviceType.c_str(), deviceNumber, nodeId);
  directNeighbors.erase(nodeId);
}

// Handles all received messages
void receivedCallback(uint32_t from, String &msg) {
  Serial.println("[NODE-%s-%d] Received from %u: %s\n", deviceType.c_str(), deviceNumber, from, msg.c_str());
  Serial.println("[NODE-%s-%d] My Node ID: %u\n", deviceType.c_str(), deviceNumber, mesh.getNodeId());

  // Process hop/seq update messages
  if (msg.startsWith("UPDATE_HOP:")) {
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    int thirdColon = msg.indexOf(':', secondColon + 1);
    int fourthColon = msg.indexOf(':', thirdColon + 1);

    int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
    uint32_t receivedSeq = msg.substring(secondColon + 1, thirdColon).toInt();
    uint32_t incomingHubId = strtoul(msg.substring(thirdColon + 1, fourthColon).c_str(), NULL, 10);
    uint8_t incomingLocalHubId = msg.substring(fourthColon + 1).toInt(); 

    if(myHubId == 0) {
      myHubId = incomingHubId;  // Set initial hub ID
      mylocalHubId = incomingLocalHubId;  // Set local hub ID
      lastSeqNum = receivedSeq;
      lastUpdateHopTime = millis();
      HopCountUpdated(receivedHop, from);
      Serial.println("[NODE-%s-%d] Initial hub set to %u with local ID %u\n", deviceType.c_str(), deviceNumber, myHubId, mylocalHubId);
    }
    // 1. If a better hop path is found (shorter path), switch to it
    else if (receivedHop + 1 < myHopCount) {
      if (myHubId != 0 && myHubId != incomingHubId) {
        // Inform old hub that this node is leaving
        String leaveMsg = "LEAVE:" + String(mesh.getNodeId());
        sendFromNormal(myHubId, leaveMsg);
        Serial.println("[NODE-%s-%d] Sent LEAVE to old hub %u\n", deviceType.c_str(), deviceNumber, myHubId);
      }
      myHubId = incomingHubId;
      lastSeqNum = receivedSeq;
      mylocalHubId = incomingLocalHubId;  // Update local hub ID
      lastUpdateHopTime = millis();
      HopCountUpdated(receivedHop, from);
      Serial.println("[NODE-%s-%d] Switched to Hub %u with better hop\n", deviceType.c_str(), deviceNumber, myHubId);
    }

    // 2. If message is from current hub, and has newer sequence
    else if (incomingHubId == myHubId) {
      if (isNewer(receivedSeq, lastSeqNum)) {
        lastSeqNum = receivedSeq;
        lastUpdateHopTime = millis();
        HopCountUpdated(receivedHop, from);
        Serial.println("[NODE-%s-%d] Seq update from same Hub %u: Seq %u\n", deviceType.c_str(), deviceNumber, myHubId, lastSeqNum);
      }
    }

    // 3. If worse hop and different hub → ignore it
    else if (incomingHubId != myHubId) {
      Serial.println("[NODE-%s-%d] Ignoring message from different hub %u\n", deviceType.c_str(), deviceNumber, incomingHubId);
      return;
    }
  }

  // If a hub requests sensor data
  else if (msg.startsWith("REQUEST:")) {
    msg.trim();  // Ensure no newline messes with parsing
    uint32_t requestingHubId = strtoul(msg.substring(8).c_str(), NULL, 10);

    if (requestingHubId != myHubId) {
      Serial.println("[NODE-%s-%d] WARNING: REQUEST from non-assigned hub %u (current myHubId = %u)\n", deviceType.c_str(), deviceNumber, requestingHubId, myHubId);
    }

    if (myHubId == 0) {
      Serial.println("[NODE-%s-%d] ERROR: No assigned hub to send sensor data to.\n", deviceType.c_str(), deviceNumber);
    }
    else{
      int sensorVal = 18;  // Simulated sensor reading  
      String sensorMsg = "DATA:" + deviceType + "-" + String(deviceNumber) +
      ":Sensor=" + String(sensorVal) +
      ":Hop=" + String(myHopCount) +
      ":Sequence=" + String(lastSeqNum) +
      ":NodeId=" + String(mesh.getNodeId()) +
      ":LocalHubId=" + String(mylocalHubId) + 
      ":Time=" + String(millis());
      sendFromNormal(myHubId, sensorMsg);
      Serial.println("[NODE-%s-%d] Sent sensor data to myHubId %u\n", deviceType.c_str(), deviceNumber, myHubId);
    }
  }

}

void setup() {
  Serial.begin(115200);
  mesh.setDebugMsgTypes(STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
}

void loop() {
  mesh.update();

  // If no UPDATE_HOP received within timeout, reset state
  if (millis() - lastUpdateHopTime > updateHopTimeout) {
    Serial.println("[NODE-%s-%d] No UPDATE_HOP received in 60 seconds. Resetting hop count and sequence.\n", deviceType.c_str(), deviceNumber);
    myHubId = 0;
    myHopCount = 255;
    lastSeqNum = 0;
    lastUpdateHopTime = millis();  // Prevent immediate repeat
  }
}
