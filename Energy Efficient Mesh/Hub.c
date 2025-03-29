#include "painlessMesh.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <map>
#include <queue>
#include <vector>
#include <algorithm>
#include <Arduino.h>

// Mesh network configuration
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

Scheduler userScheduler;
painlessMesh mesh;

std::map<uint32_t, int> nodeHopCounts; // Maps node IDs to their hop counts
std::queue<uint32_t> requestQueue;     // Global queue for round-robin scheduling
// Queue to store data messages from normal nodes for future processing
std::queue<String> dataQueue;

uint32_t gatewayId = 0;

// Handshake state variables
bool ackReceived = false;
unsigned long ackRequestTime = 0;

// Task: Broadcast HUB_ID every 3 minutes (180 seconds)
Task taskBroadcastHubId(TASK_SECOND * 30, TASK_FOREVER, []() {
  String hubMsg = "HUB_ID:" + String(mesh.getNodeId());
  mesh.sendBroadcast(hubMsg);
  Serial.println("[BROADCAST] " + hubMsg);
});

// Task: Broadcast UPDATE_HOP:0 every 3 minutes (180 seconds)
Task taskBroadcastUpdateHop(TASK_SECOND * 75, TASK_FOREVER, []() {
  String updateMsg = "UPDATE_HOP:0";
  mesh.sendBroadcast(updateMsg);
  Serial.println("[BROADCAST] " + updateMsg);
});

Task taskRequestData(TASK_SECOND * 75, TASK_FOREVER, []() {
  Serial.println("[HUB] Initiating data request cycle...");
  
  // If the global requestQueue is empty, rebuild it
  if (requestQueue.empty()) {
    std::vector<std::pair<uint32_t, int>> nodes(nodeHopCounts.begin(), nodeHopCounts.end());
    // Sort nodes in decreasing order (farthest first)
    std::sort(nodes.begin(), nodes.end(), [](const auto &a, const auto &b) {
      return a.second > b.second;
    });
    for (const auto &p : nodes) {
      requestQueue.push(p.first);
    }
    Serial.printf("[HUB] Rebuilt request queue with %lu nodes\n", requestQueue.size());
  }
  
  if (!requestQueue.empty()) {
    // Get the node at the front of the queue
    uint32_t targetNode = requestQueue.front();
    requestQueue.pop();
    
    String reqMsg = "REQUEST_SEND";
    mesh.sendSingle(targetNode, reqMsg);
    Serial.printf("[HUB] Requesting data from node %u (hop count %d)\n",
                  targetNode, nodeHopCounts[targetNode]);
    
    // Round-robin: push the target node back to the queue
    requestQueue.push(targetNode);
  } else {
    Serial.println("[HUB] No nodes available for data request.");
  }
});

Task taskSendDataWithAck(TASK_SECOND * 30, TASK_FOREVER, []() {
  if (gatewayId == 0) {
    Serial.println("[HUB] Gateway ID not known; cannot initiate handshake.");
    return;
  }
  // Initiate handshake: send ACK_REQUEST to gateway
  ackReceived = false;
  ackRequestTime = millis();
  String ackReq = "ACK";
  mesh.sendSingle(gatewayId, ackReq);
  Serial.printf("[HUB] Sent ACK_REQUEST to gateway (%u).\n", gatewayId);
  
  // Schedule a one-shot check for ACK in 10 seconds
  userScheduler.addTask(taskCheckAck);
  taskCheckAck.enableDelayed(TASK_SECOND * 20);
});

Task taskCheckAck(0, TASK_ONCE, []() {
  if (!ackReceived) {
    Serial.println("[HUB] No ACK received within 10 sec; scheduling retry in 15 sec.");
    // Schedule a retry (one-shot) in 15 seconds
    userScheduler.addTask(taskSendDataRetry);
    taskSendDataRetry.enableDelayed(TASK_SECOND * 15);
  } else {
    // ACK was received; send all queued data to gateway.
    Serial.println("[HUB] ACK received. Sending queued messages to gateway...");
    while (!messageQueue.empty()) {
      String dataMsg = messageQueue.front();
      messageQueue.pop();
      mesh.sendSingle(gatewayId, dataMsg);
      Serial.printf("[HUB] Sent to gateway (%u): %s\n", gatewayId, dataMsg.c_str());
    }

    ackRecieved = false;
  }
});

// New connection callback: when a node connects, send it the hub id and initial hop count update
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[HUB] New connection: node %u\n", nodeId);
  
  String initMsg = "HUB_ID:" + String(mesh.getNodeId());
  mesh.sendSingle(nodeId, initMsg);
  
  String updateMsg = "UPDATE_HOP:0";
  mesh.sendSingle(nodeId, updateMsg);
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
    if (secondColon != -1) {
      int receivedHop = msg.substring(firstColon + 1, secondColon).toInt();
      uint32_t senderId = strtoul(msg.substring(secondColon + 1).c_str(), NULL, 10);
      if (nodeHopCounts.find(senderId) == nodeHopCounts.end() || receivedHop < nodeHopCounts[senderId]) {
        nodeHopCounts[senderId] = receivedHop;
        Serial.printf("[HUB] Node %u updated hop count to %d\n", senderId, receivedHop);
      }
    }
  }

  else if (msg.startsWith("GATEWAY:")) {
    // Use strtoul to avoid overflow issues
    uint32_t id = strtoul(msg.substring(8).c_str(), NULL, 10);
    if (id != gatewayId) {
      gatewayId = id;
      Serial.printf("[HUB] Updated gateway ID to %u\n", gatewayId);
    }
  }
  // Handle data messages from normal nodes:
  // Expected format: "DATA:<deviceType>-<deviceNumber>:Sensor=<sensorVal>:Hop=<hopCount>"
  else if (msg.startsWith("DATA:")) {
    int firstColon = msg.indexOf(':'); // after "DATA"
    int secondColon = msg.indexOf(':', firstColon + 1);
    if (firstColon != -1 && secondColon != -1) {
      String deviceInfo = msg.substring(firstColon + 1, secondColon);
      int dashIndex = deviceInfo.indexOf('-');
      String deviceType = "";
      int deviceNumber = 0;
      if (dashIndex != -1) {
        deviceType = deviceInfo.substring(0, dashIndex);
        deviceNumber = deviceInfo.substring(dashIndex + 1).toInt();
      }
      int sensorIndex = msg.indexOf("Sensor=", secondColon);
      int hopIndex = msg.indexOf(":Hop=", sensorIndex);
      int sensorValue = 0;
      int hopCount = 0;
      if (sensorIndex != -1 && hopIndex != -1) {
        sensorValue = msg.substring(sensorIndex + 7, hopIndex).toInt();
        hopCount = msg.substring(hopIndex + 5).toInt();
      }
      Serial.printf("[HUB] Received Data from %s-%d: Sensor=%d, Hop=%d\n",
                    deviceType.c_str(), deviceNumber, sensorValue, hopCount);
      // Save the entire message (or a formatted version) into the queue for future use
      dataQueue.push(msg);
      Serial.printf("[HUB] Data message queued. Queue size: %lu\n", dataQueue.size());
    }
  }

  else if (msg.startsWith("ACK")) {
    unsigned long ackTime = msg.substring(4).toInt();
    // Accept any ACK if we are waiting
    if (!ackReceived) {
      ackReceived = true;
      Serial.printf("[HUB] Received ACK from gateway with timestamp %lu\n", ackTime);
    }
  }
  // Optionally, handle other message types here.
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Hub Node");

  // Initialize mesh network
 
  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION | COMMUNICATION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.setRoot(true);
  Serial.printf("[HUB] My Node ID: %u\n", mesh.getNodeId());
  
  // Set up callbacks
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);

  
  // Add and enable broadcast tasks (each running every 3 minutes)
  userScheduler.addTask(taskBroadcastHubId);
  taskBroadcastHubId.enable();
  
  userScheduler.addTask(taskBroadcastUpdateHop);
  taskBroadcastUpdateHop.enable();

  userScheduler.addTask(taskRequestData);
  taskRequestData.enable();

  userScheduler.addTask(taskSendDataToGateway);
  taskSendDataToGateway.enable();
}

void loop() {
  mesh.update();
}
