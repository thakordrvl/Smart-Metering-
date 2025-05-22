#include "painlessMesh.h"
#include <map>
#include <queue>
#include <vector>
#include <algorithm>
#include <Arduino.h>
#include <set>

// Mesh network configuration
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MAX_SEQ 1000           // Maximum sequence number before wrapping

Scheduler userScheduler;
painlessMesh mesh;

std::map<uint32_t, int> nodeHopCounts; // Maps node IDs to their hop counts
std::queue<uint32_t> requestQueue;     // Global queue for round-robin scheduling
// Queue to store data messages from normal nodes for future processing
std::queue<String> dataQueue;
std::queue<String> dataQueueBackup; // Backup queue for data message
std::set<uint32_t> directNeighbors;

uint32_t gatewayId = 0;
uint32_t sequenceNumber = 1;  // Global sequence number (1 to 1000)

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

void generateRequestList() {
  std::vector<std::pair<uint32_t, int>> nodes(nodeHopCounts.begin(), nodeHopCounts.end());
  std::sort(nodes.begin(), nodes.end(), [](const auto &a, const auto &b) {
    return a.second > b.second;
  });
  while (!requestQueue.empty())
    requestQueue.pop();
  for (const auto &p : nodes) {
    requestQueue.push(p.first);
  }
  Serial.printf("[HUB] Rebuilt request queue with %lu nodes\n", requestQueue.size());
}

void SendDatatoGateway(){

  Serial.println("[HUB] Sending backup data to Gateway...");

  while(dataQueueBackup.size()){
    if(gatewayId == 0){
      Serial.println("[HUB] No gateway ID set, cannot send data.");
      break;
    }else{
      String backupMsg = dataQueueBackup.front();
      dataQueueBackup.pop();
      mesh.sendSingle(gatewayId, backupMsg);
      Serial.printf("[HUB] (Backup) Sent to gateway (%u): %s\n", gatewayId, backupMsg.c_str());
    }
  }

  while(dataQueue.size()){
    if(gatewayId == 0){
      Serial.println("[HUB] No gateway ID set, cannot send data.");
      break;
    }else{
      String Msg = dataQueue.front();
      dataQueue.pop();
      dataQueueBackup.push(Msg);
      mesh.sendSingle(gatewayId, Msg);
      Serial.printf("[HUB] (Backup) Sent to gateway (%u): %s\n", gatewayId, Msg.c_str());
    }
  }
}

// Task: Broadcast HUB_ID every 3 minutes (180 seconds)
Task taskBroadcastUpdateHop(TASK_SECOND * 30, TASK_FOREVER, []() {
  String updateMsg = "UPDATE_HOP:0:" + String(sequenceNumber) + ":" + String(mesh.getNodeId()); // Append hub ID
  sendToAllNeighbors(updateMsg);
  sequenceNumber = (sequenceNumber % MAX_SEQ) + 1;
});


Task taskRequestData(TASK_SECOND * 60, TASK_FOREVER, []() {
  Serial.println("[HUB] Initiating data request cycle..."); 
  
  // If the global requestQueue is empty, rebuild it
  if (requestQueue.empty()) {
    generateRequestList();
  }

  while(!requestQueue.empty()) {
    // Get the node at the front of the queue
    uint32_t targetNode = requestQueue.front();
    requestQueue.pop();
    
    String reqMsg = "REQUEST:" + String(mesh.getNodeId());
    mesh.sendSingle(targetNode, reqMsg);
    Serial.printf("[HUB] Requesting data from node %u (hop count %d)\n",targetNode, nodeHopCounts[targetNode]);
  } 
});

// New connection callback: when a node connects, send it the hub id and initial hop count update
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[HUB] New connection: node %u\n", nodeId);
  // Add the new node to the direct neighbors list
  directNeighbors.insert(nodeId);

  String initMsg = "HUB_ID:" + String(mesh.getNodeId());
  mesh.sendSingle(nodeId, initMsg);
  
  String updateMsg = "UPDATE_HOP:0:" + String(sequenceNumber);
  mesh.sendSingle(nodeId, updateMsg);
}


void droppedConnectionCallback(uint32_t nodeId) {
  Serial.print("Connection dropped: ");
  Serial.println(nodeId);
  directNeighbors.erase(nodeId);
}

// Received callback: Process incoming messages from normal nodes.
// Two message types are handled:
// 1. "UPDATE_HOP_HUB:<hopcount>:<nodeId>"
// 2. "DATA:<deviceType>-<deviceNumber>:Sensor=<sensorVal>:Hop=<hopCount>"
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[HUB] Received from %u: %s\n", from, msg.c_str());
  
  // Handle hop count update messages:
  if (msg.startsWith("UPDATE_HOP_HUB:")) {
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);

    if (secondColon != -1 && thirdColon != -1) {
        int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
        uint32_t senderId = strtoul(msg.substring(secondColon + 1, thirdColon).c_str(), NULL, 10);
        nodeHopCounts[senderId] = receivedHop;
    }
  }

  else if (msg.startsWith("GATEWAY:")) {
    // Use strtoul to avoid overflow issues
    uint32_t id = strtoul(msg.substring(8).c_str(), NULL, 10);

    gatewayId = id;
    Serial.printf("[HUB] Updated gateway ID to %u\n", gatewayId);
    string hubMsg = "HUB_ID:" + String(mesh.getNodeId());
    mesh.sendSingle(gatewayId, hubMsg);
    
  }
  // Handle data messages from normal nodes:
  // Expected format: "DATA:<deviceType>-<deviceNumber>:Sensor=<sensorVal>:Hop=<hopCount>"
  else if (msg.startsWith("DATA:")) {
      Serial.printf("[HUB] Data message received: %s\n", msg.c_str());
      dataQueue.push(msg);
      Serial.printf("[HUB] Data message queued. Queue size: %lu\n", dataQueue.size());
  }

  else if (msg.startsWith("DATA_REQUEST:")) {
      SendDatatoGateway();
  }

  else if (msg.startsWith("LEAVE:")) {
    uint32_t leavingNode = msg.substring(6).toInt();
    nodeHopCounts.erase(leavingNode);
    Serial.printf("[HUB] Node %u has left this hub\n", leavingNode);
  }
}
  // Optionally, handle other message types here.


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Hub Node");

  // Initialize mesh network
 
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  Serial.printf("[HUB] My Node ID: %u\n", mesh.getNodeId());
  
  // Set up callbacks
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
  
  // Add and enable broadcast tasks (each running every 3 minutes)
  userScheduler.addTask(taskBroadcastUpdateHop);
  taskBroadcastUpdateHop.enable();

  userScheduler.addTask(taskRequestData);
  taskRequestData.enable();
}

void loop() {
  mesh.update();
}