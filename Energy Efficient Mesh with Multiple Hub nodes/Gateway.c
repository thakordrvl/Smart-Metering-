#include "painlessMesh.h"
// #include <WiFi.h>          // For ESP8266; use <WiFi.h> for ESP32
// #include <HTTPClient.h>    // For ESP8266; use <HTTPClient.h> for ESP32
#include <ESP8266WiFi.h>          // For ESP8266; use <WiFi.h> for ESP32
#include <ESP8266HTTPClient.h>    // For ESP8266; use <HTTPClient.h> for ESP32
#include <queue>

//*************** Mesh Configuration *******************
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

// Hotspot credentials (laptop hotspot)
const char* hotspotSSID = "drvl";
const char* hotspotPassword = "hehehaha";

// Server endpoint (for HTTP POST, adjust if needed)
const char* SERVER_URL = "http://192.168.137.1:5000/data";

// Define operational states
enum State {
  MESH_PHASE,
  UPLOAD_PHASE
};

State currentState = MESH_PHASE;
unsigned long stateStartTime = 0;
const unsigned long meshPhaseDuration = 60000;   // 60 seconds for mesh phase
const unsigned long uploadPhaseDuration = 15000; // 15 seconds for upload phase
uint32_t myHubId = 0;  // Set this when you receive HUB_ID

// Global objects
Scheduler userScheduler;
painlessMesh mesh;
std::queue<String> messageQueue;  // Queue for storing incoming messages

// WiFi and HTTP objects for upload phase
WiFiClient wifiClient;



Task taskGatewayBroadcast(TASK_SECOND * 20, TASK_FOREVER, []() {
  if(myHubId == 0) {
    Serial.println("[GATEWAY] No hub ID set, cannot send broadcast.");
    return;
  }
  else{
    String msg = "GATEWAY:" + String(mesh.getNodeId());
    mesh.sendSingle(myHubId,msg);
    Serial.printf("[GATEWAY] Sending to hub (%u): %s\n", myHubId, msg.c_str());
    String req = "DATA_REQUEST:" + String(mesh.getNodeId());
    mesh.sendSingle(myHubId,req);
    Serial.printf("[GATEWAY] Second DATA_REQUEST sent: %s\n", req.c_str());
  }
});

// Mesh callback: store any received messages in the queue
void receivedCallback(uint32_t from, String &msg) {
  if (msg.startsWith("DATA")){
  Serial.printf("[GATEWAY] Received from %u: %s\n", from, msg.c_str());
  messageQueue.push(msg);
  }

  else if (msg.startsWith("HUB_ID:")) {
    uint32_t newHubId = msg.substring(7).toInt();
    if (newHubId != myHubId) {
      myHubId = newHubId;
      // Serial.printf("[NODE] Updated Hub ID to %u, broadcasting: %s\n", myHubId, hubMsg.c_str());
    }
  }
}

// Switch from Mesh Phase to Upload Phase
void switchToUploadPhase() {
  taskGatewayBroadcast.disable();
  Serial.println("[SWITCH] Transitioning to UPLOAD PHASE");
  // Stop mesh operations to prevent further message reception
  mesh.stop();
  
  // Switch WiFi mode to STA and connect to the hotspot
  WiFi.mode(WIFI_STA);
  WiFi.begin(hotspotSSID, hotspotPassword);
  
  int retryCount = 0;
  while(WiFi.status() != WL_CONNECTED && retryCount < 60) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("[UPLOAD] Connected to hotspot. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("[UPLOAD] Failed to connect to hotspot.");
  }
  
  stateStartTime = millis();
  uploadData();
  currentState = UPLOAD_PHASE;
}


// Switch from Upload Phase back to Mesh Phase
void switchToMeshPhase() {
  currentState = MESH_PHASE;
  Serial.println("[SWITCH] Transitioning back to MESH PHASE");
  // Disconnect from hotspot
  WiFi.disconnect();
  // Switch WiFi mode to AP (for mesh)
  WiFi.mode(WIFI_AP);
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.setContainsRoot(true);
  userScheduler.addTask(taskGatewayBroadcast);
  taskGatewayBroadcast.enable();
  Serial.printf("[GATEWAY] Node ID: %u\n", mesh.getNodeId());
  stateStartTime = millis();
}
// Upload one message from the queue via HTTP POST
void uploadData() {
  if (WiFi.status() == WL_CONNECTED) {
    while(!messageQueue.empty()) {
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
    switchToMeshPhase();
    
  } else {
    Serial.println("[UPLOAD] WiFi not connected.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Gateway/Upload Cycle");
  // Start mesh network in AP mode for mesh operation
  // Start in Mesh Phase.
  switchToMeshPhase();
}

void loop() {
  if (currentState == MESH_PHASE) {
    mesh.update();
    // Remain in Mesh Phase for the designated duration.
    if (millis() - stateStartTime > meshPhaseDuration) {
      switchToUploadPhase();
    }
  }
  else if (currentState == UPLOAD_PHASE) {
    // In Upload Phase, call uploadData() repeatedly
    // Remain in Upload Phase for the designated duration.
    if (millis() - stateStartTime > uploadPhaseDuration) {
      switchToMeshPhase();
    }
  }
}