/*Code Written and Created by Dhruvil Thakor. 
If you want to use this code, I would suggest you to go through the commit history and documentation of PainlessMesh
You are welcome.*/

#include "painlessMesh.h"
#include <map>
#include <queue>
#include <vector>
#include <algorithm>
#include <Arduino.h>
#include <set>

//*************** Mesh Configuration *******************
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555
#define MAX_SEQ 1000  // Sequence number wraps after 1000

Scheduler userScheduler;
painlessMesh mesh;

// Track hop counts of normal nodes: nodeId -> hopCount
std::map<uint32_t, int> nodeHopCounts;

// Round-robin data polling queue
std::queue<uint32_t> requestQueue;

// Buffers for received data from normal nodes
std::queue<String> dataQueue;
std::queue<String> dataQueueBackup;  // Backup copy in case gateway is down

std::set<uint32_t> directNeighbors;  // Immediate mesh neighbors

uint32_t gatewayId = 0;         // Last known gateway
uint32_t sequenceNumber = 1;    // Hub's own sequence counter
uint8_t localHubId = 1;  // Unique ID per hub (manually assigned)

bool sendFromHub(uint32_t targetId, const String& msg) {
  bool sent = mesh.sendSingle(targetId, msg);
  Serial.printf("[HUB-%d] Sent to %u? %s | Message: %s\n", localHubId, targetId, sent ? "Yes" : "No", msg.c_str());

  if (!sent) {
    auto list = mesh.getNodeList(true);
    Serial.print("Known nodes: ");
    for (auto n : list) Serial.print(n), Serial.print(" ");
    Serial.println();

    Serial.printf("[HUB-%d] [MESSAGE FAILED] %s\n", localHubId, msg.c_str());
    if (std::find(list.begin(), list.end(), targetId) == list.end()) {
      Serial.printf("[HUB-%d] [ERROR] Target %u not found in routing table!\n", localHubId, targetId);
    } else {
      Serial.printf("[HUB-%d] [WARN] Target %u is known but message failed to send.\n", localHubId, targetId);
    }
  }

  return sent;
}

// Send a message to all direct neighbors, optionally excluding a node
void sendToAllNeighbors(String &msg, uint32_t excludeNode) {
    Serial.printf("[HUB-%d] Broadcasting message: %s\n", localHubId, msg.c_str());
    
  for (uint32_t node : directNeighbors) {
    if (node != excludeNode) {
      Serial.printf("[HUB-%d] Sending to node: %u\n", localHubId, node);
      sendFromHub(node, msg);
    }
  }
}

// Rebuild the request queue for round-robin polling
void generateRequestList() {
  std::vector<std::pair<uint32_t, int>> nodes(nodeHopCounts.begin(), nodeHopCounts.end());

  // Sort nodes by hop count (descending)
  std::sort(nodes.begin(), nodes.end(), [](const auto &a, const auto &b) {
    return a.second > b.second;
  });

  // Clear and refill request queue
  while (!requestQueue.empty())
    requestQueue.pop();
    
  for (const auto &p : nodes) {
    requestQueue.push(p.first);
  }

  Serial.printf("[HUB-%d] Rebuilt request queue with %lu nodes\n", localHubId, requestQueue.size());
}

// Send buffered data to gateway (backup and live queues)
void SendDatatoGateway() {
  Serial.printf("[HUB-%d] Sending backup data to Gateway...\n", localHubId);

  if (dataQueue.empty() && dataQueueBackup.empty()) {
    Serial.printf("[HUB-%d] No data to send to gateway.\n", localHubId);
    String msg = "NO_DATA:LocalHubId=" + String(localHubId);
    sendFromHub(gatewayId, msg);
    return;
  }
  // First try sending older backup messages
  while (!dataQueueBackup.empty()) {
    if (gatewayId == 0) {
      Serial.printf("[HUB-%d] No gateway ID set, cannot send data.\n", localHubId);
      break;
    } else {
      String backupMsg = dataQueueBackup.front();
      dataQueueBackup.pop();
      sendFromHub(gatewayId, backupMsg);
      Serial.printf("[HUB-%d] (Backup) Sent to gateway (%u): %s\n", localHubId, gatewayId, backupMsg.c_str());
    }
  }

  // Then send new data, also backing it up in case it fails
  while (!dataQueue.empty()) {
    if (gatewayId == 0) {
      Serial.printf("[HUB-%d] No gateway ID set, cannot send data.\n", localHubId);
      break;
    } else {
      String Msg = dataQueue.front();
      dataQueue.pop();
      dataQueueBackup.push(Msg);
      sendFromHub(gatewayId, Msg);
      Serial.printf("[HUB-%d] Sent to gateway (%u): %s\n", localHubId, gatewayId, Msg.c_str());
    }
  }
}

// Periodically broadcast an UPDATE_HOP message to neighbors
Task taskBroadcastUpdateHop(TASK_SECOND * 30, TASK_FOREVER, []() {
  String updateMsg = "UPDATE_HOP:0:" + String(sequenceNumber) + ":" + String(mesh.getNodeId()) + ":" + String(localHubId);
  sendToAllNeighbors(updateMsg, 0);  // Broadcast to all neighbors
  sequenceNumber = (sequenceNumber % MAX_SEQ) + 1;  // Wrap after MAX_SEQ
});

// Request data from normal nodes in round-robin fashion
Task taskRequestData(TASK_SECOND * 60, TASK_FOREVER, []() {
  Serial.printf("[HUB-%d] Initiating data request cycle...\n", localHubId);

  if (requestQueue.empty()) {
    generateRequestList();
  }

  while (!requestQueue.empty()) {
    uint32_t targetNode = requestQueue.front();
    requestQueue.pop();

    String reqMsg = "REQUEST:" + String(mesh.getNodeId());  // Identify self in request
    sendFromHub(targetNode, reqMsg);
    Serial.printf("[HUB-%d] Requesting data from node %u (hop count %d)\n", localHubId, targetNode, nodeHopCounts[targetNode]);
  }
});

// Called when a new neighbor connects
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[HUB-%d] New connection: node %u\n", localHubId, nodeId);
  directNeighbors.insert(nodeId);  // Track neighbor

  // Send identity and sequence
  String initMsg = "HUB_ID:" + String(mesh.getNodeId());
  sendFromHub(nodeId, initMsg);

  String updateMsg = "UPDATE_HOP:0:" + String(sequenceNumber) + ":" + String(mesh.getNodeId()) + ":" + String(localHubId);
  sendFromHub(nodeId, updateMsg);
}

// Called when a connection is dropped
void droppedConnectionCallback(uint32_t nodeId) {
  Serial.print("Connection dropped: ");
  Serial.println(nodeId);
  directNeighbors.erase(nodeId);
}

// Main message handler
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[HUB-%d] Received from %u: %s\n", localHubId, from, msg.c_str());

  // Normal node is reporting its hop count
  if (msg.startsWith("UPDATE_HOP_HUB:")) {
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    int thirdColon = msg.indexOf(':', secondColon + 1); 
    int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
    uint32_t senderId = strtoul(msg.substring(secondColon + 1, thirdColon).c_str(), NULL, 10);
    nodeHopCounts[senderId] = receivedHop;
  }

  // Gateway is announcing itself
  else if (msg.startsWith("GATEWAY:")) {
    uint32_t id = strtoul(msg.substring(8).c_str(), NULL, 10);
    gatewayId = id;
   Serial.printf("[HUB-%d] Updated gateway ID to %u\n", localHubId, gatewayId);
    // Send identity back to gateway
    String hubMsg = "HUB_ID:" + String(mesh.getNodeId());
    sendFromHub(gatewayId, hubMsg);
  }

  // Received sensor data from normal node
  else if (msg.startsWith("DATA:")) {
    Serial.printf("[HUB-%d] Data message received: %s\n", localHubId, msg.c_str());
    dataQueue.push(msg);
    Serial.printf("[HUB-%d] Data message queued. Queue size: %lu\n", localHubId, dataQueue.size());
  }

  // Gateway is requesting data dump
  else if (msg.startsWith("DATA_REQUEST:")) {
    Serial.printf("[HUB-%d] Data request received from gateway %u\n", localHubId, from);
    SendDatatoGateway();
  }

  // A node informs it’s leaving this hub
  else if (msg.startsWith("LEAVE:")) {
    uint32_t leavingNode = msg.substring(6).toInt();
    nodeHopCounts.erase(leavingNode);
    Serial.printf("[HUB-%d] Node %u has left this hub\n", localHubId, leavingNode);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Hub Node");

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  Serial.printf("[HUB-%d] My Node ID: %u\n", localHubId, mesh.getNodeId());

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);

  userScheduler.addTask(taskBroadcastUpdateHop);
  taskBroadcastUpdateHop.enable();

  userScheduler.addTask(taskRequestData);
  taskRequestData.enable();
}

void loop() {
  mesh.update();
}