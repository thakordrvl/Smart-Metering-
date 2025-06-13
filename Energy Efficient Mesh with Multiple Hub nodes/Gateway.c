#include "painlessMesh.h"
// For ESP8266 use:
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <queue>
#include <set>

//*************** Mesh Configuration *******************
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

// WiFi hotspot credentials (used during upload phase)
const char* hotspotSSID = "drvl";
const char* hotspotPassword = "hehehaha";

// Server URL for uploading data
const char* SERVER_URL = "http://192.168.137.1:5000/data";

// Mesh state machine control
enum State {
  MESH_PHASE,
  UPLOAD_PHASE
};

State currentState = MESH_PHASE;
unsigned long stateStartTime = 0;
const unsigned long meshPhaseDuration = 60000;   // Duration for mesh operation
const unsigned long uploadPhaseDuration = 15000; // Duration for HTTP upload

// Store all discovered hub IDs
std::set<uint32_t> hubIds;

// Global mesh and scheduling objects
Scheduler userScheduler;
painlessMesh mesh;
std::queue<String> messageQueue;  // Queue to hold data messages received from hubs

WiFiClient wifiClient;  // Used for HTTP communication

bool sendFromGateway(uint32_t targetId, const String& msg) {
  bool sent = mesh.sendSingle(targetId, msg);
  Serial.printf("[GATEWAY] Sent to %u? %s | Message: %s\n", targetId, sent ? "Yes" : "No", msg.c_str());

  if (!sent) {
    auto list = mesh.getNodeList(true);
    Serial.print("Known nodes: ");
    for (auto n : list) Serial.print(n), Serial.print(" ");
    Serial.println();

    Serial.printf("[GATEWAY] [MESSAGE FAILED] %s\n", msg.c_str());
    if (std::find(list.begin(), list.end(), targetId) == list.end()) {
      Serial.printf("[GATEWAY] [ERROR] Target %u not found in routing table!\n", targetId);
    } else {
      Serial.printf("[GATEWAY] [WARN] Target %u is known but message failed to send.\n", targetId);
    }
  }

  return sent;
}

// Task 1: Broadcast this gateway's presence so hubs can respond
Task taskBroadcastGatewayId(TASK_SECOND * 30, TASK_FOREVER, []() {
  String msg = "GATEWAY:" + String(mesh.getNodeId());
  mesh.sendBroadcast(msg);
  Serial.printf("[GATEWAY] Broadcasting: %s\n", msg.c_str());
});

// Task 2: Request data from all known hubs
Task taskSendDataRequests(TASK_SECOND * 45, TASK_FOREVER, []() {
  for (auto hubId : hubIds) {
    String req = "DATA_REQUEST:" + String(mesh.getNodeId());
    sendFromGateway(hubId, req);
    Serial.printf("[GATEWAY] Sent DATA_REQUEST to hub %u: %s\n", hubId, req.c_str());
  }
});

// Mesh callback: handle all incoming messages
void receivedCallback(uint32_t from, String &msg) {
  // Data from hubs
  if (msg.startsWith("DATA")) {
    Serial.printf("[GATEWAY] Received from %u: %s\n", from, msg.c_str());
    messageQueue.push(msg);
  }
  // Response from hub after gateway broadcast
  else if (msg.startsWith("HUB_ID:")) {
    uint32_t newHubId = strtoul(msg.substring(7).c_str(), NULL, 10);
    if (hubIds.find(newHubId) == hubIds.end()) {
      hubIds.insert(newHubId);
      Serial.printf("[GATEWAY] New hub ID registered: %u\n", newHubId);
    }
  }
}

// Transition to UPLOAD phase: stop mesh and upload queued data via WiFi
void switchToUploadPhase() {
  taskBroadcastGatewayId.disable();
  taskSendDataRequests.disable();

  Serial.println("[SWITCH] Transitioning to UPLOAD PHASE");

  mesh.stop(); // stop all mesh operations during upload

  WiFi.mode(WIFI_STA);
  WiFi.begin(hotspotSSID, hotspotPassword);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 60) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("[UPLOAD] Connected to hotspot. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("[UPLOAD] Failed to connect to hotspot.");
  }

  stateStartTime = millis();
  uploadData();  // Begin uploading all queued data
  currentState = UPLOAD_PHASE;
}

// Transition back to MESH phase: reconnect to mesh and resume polling
void switchToMeshPhase() {
  currentState = MESH_PHASE;
  Serial.println("[SWITCH] Transitioning back to MESH PHASE");

  WiFi.disconnect(); // leave WiFi STA mode

  WiFi.mode(WIFI_AP); // re-enter mesh mode
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  // Resume both gateway broadcast and hub polling tasks
  userScheduler.addTask(taskBroadcastGatewayId);
  userScheduler.addTask(taskSendDataRequests);
  taskBroadcastGatewayId.enable();
  taskSendDataRequests.enable();

  Serial.printf("[GATEWAY] Node ID: %u\n", mesh.getNodeId());

  stateStartTime = millis();
}

// Upload all queued messages via HTTP POST
void uploadData() {
  if (WiFi.status() == WL_CONNECTED) {
    while (!messageQueue.empty()) {
      String msg = messageQueue.front();
      messageQueue.pop();

      String payload = "{\"data\":\"" + msg + "\"}";
      Serial.println("[UPLOAD] Sending payload: " + payload);

      HTTPClient http;
      http.begin(wifiClient, SERVER_URL);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(payload);
      if (httpResponseCode > 0) {
        Serial.printf("[UPLOAD] HTTP Response: %d\n", httpResponseCode);
      } else {
        Serial.printf("[UPLOAD] HTTP POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();
    }

    Serial.println("[UPLOAD] Queue is empty now.");
    switchToMeshPhase();  // Return to mesh phase after successful upload
  } else {
    Serial.println("[UPLOAD] WiFi not connected.");
  }
}

// Initialize and enter mesh phase
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Gateway/Upload Cycle");
  switchToMeshPhase(); // Start directly in mesh mode
}

// State machine handler
void loop() {
  if (currentState == MESH_PHASE) {
    mesh.update();

    // Check if it's time to switch to upload phase
    if (millis() - stateStartTime > meshPhaseDuration) {
      switchToUploadPhase();
    }
  }
  else if (currentState == UPLOAD_PHASE) {
    // If time's up, return to mesh mode even if upload failed
    if (millis() - stateStartTime > uploadPhaseDuration) {
      switchToMeshPhase();
    }
  }
}
