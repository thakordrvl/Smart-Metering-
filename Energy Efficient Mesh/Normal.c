#include "painlessMesh.h"

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

Scheduler userScheduler;
painlessMesh mesh;

// Device configuration (set before upload)
const String deviceType = "ESP8266";
const int deviceNumber = 1; // adjust for each node

// Local state variables
uint32_t myHubId = 0; // the current hub node id received in update
int myHopCount = 256; // initially large


Task taskResetHopCount(TASK_SECOND * 60, TASK_FOREVER, []() {
  myHopCount = 256; // reset hop count to a large value
  // Serial.println("[BROADCAST] " + hubMsg);
});


Task CheckNeighbour(TASK_SECOND * 15, TASK_FOREVER, [](){
  if(myHopCount == 256){
    string help_msg = "HELP_ME";
    mesh.sendBroadcast(help_msg);
  }
});

// When a new node connects to this node, send it the hub id and the current hop count
void newConnectionCallback(uint32_t nodeId) {
  // First, send hub id message (retrieved directly using mesh.getNodeId())
  String hubMsg = "HUB_ID:" + String(myHubId);
  mesh.sendSingle(nodeId, hubMsg);
  // Serial.printf("[NODE] Sent HUB_ID to node %u: %s\n", nodeId, hubMsg.c_str());
  
  // Then, send the current hop count message
  String updateMsg = "UPDATE_HOP:" + String(myHopCount);
  mesh.sendSingle(nodeId, updateMsg);
  // Serial.printf("[NODE] Sent UPDATE_HOP to node %u: %s\n", nodeId, updateMsg.c_str());
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
    int receivedHop = msg.substring(firstColon+1).toInt();
    // Update our hop count to receivedHop+1 if that's lower than our current value
    if ((receivedHop + 1) <= myHopCount) {
      myHopCount = receivedHop + 1;
      String broadcastMsg = "UPDATE_HOP:" + String(myHopCount);
      mesh.sendBroadcast(broadcastMsg);
      Serial.printf("[NODE] Updated hop count to %d, broadcasting: %s\n", myHopCount, broadcastMsg.c_str());
      
      // Also send a hub-specific update: "UPDATE_HOP_HUB:<hopcount>:<nodeId>"
      if (myHubId != 0) {
        String hubUpdateMsg = "UPDATE_HOP_HUB:" + String(myHopCount) + ":" + String(mesh.getNodeId());
        mesh.sendSingle(myHubId, hubUpdateMsg);
        Serial.printf("[NODE] Sent hub-specific update: %s\n", hubUpdateMsg.c_str());
      }
    }
  }

  else if (msg.startsWith("REQUEST")) {
    int sensorVal = 18;
    String sensorMsg = "DATA:" + deviceType + "-" + String(deviceNumber) +
    ":Sensor=" + String(sensorVal) +
    ":Hop=" + String(myHopCount) +
    ":Time=" + String(millis());
    if (myHubId != 0) {
      mesh.sendSingle(myHubId, sensorMsg);
      Serial.println("[NODE] Sent sensor data on REQUEST_SEND");
    } else {
      Serial.println("[NODE] Hub ID not set, cannot respond to REQUEST_SEND");
    }
  } 

  else if(msg.startsWith("HELP_ME")){
    String updateMsg = "UPDATE_HOP:" + String(myHopCount);
    mesh.sendSingle(from, updateMsg);
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
