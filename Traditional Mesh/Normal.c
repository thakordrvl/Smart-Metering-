#include "painlessMesh.h"

#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

Scheduler userScheduler;  // Task scheduler
painlessMesh mesh;        // Mesh network object

// Global variable to store the current gateway ID.
uint32_t storedGatewayId = 0;
// Timestamp of the last received gateway broadcast.
unsigned long lastGatewayReceivedTime = 0;
// Timeout in milliseconds to consider the gateway offline.
const unsigned long gatewayBroadcastTimeout = 30000; // 30 seconds

//*************** Device Configuration ********************
// Change deviceNumber for each device before uploading.
const String deviceType = "ESP8266";  // Change to "ESP8266" if applicable
const int deviceNumber = 1;         // Change this for each device (e.g., 1, 2, 3, ...)

//*************** Task: Send Sensor Data ********************
// This task reads sensor data and sends it directly to the gateway,
// but only if the gateway info is recent. If the stored gateway ID is stale,
// it clears the value and prints a message.
Task taskSendSensorData(TASK_SECOND * 10, TASK_FOREVER, []() {
  // Check if the stored gateway broadcast is stale.
  if (storedGatewayId != 0 && (millis() - lastGatewayReceivedTime) > gatewayBroadcastTimeout) {
    storedGatewayId = 0;
  }
  
  if (storedGatewayId != 0) {
    int sensorValue = analogRead(A0);  // Read sensor value from A0 (replace with actual sensor read)
    String msg = "Sensor value from " + deviceType + "-" + String(deviceNumber) + 
                 " (" + String(mesh.getNodeId()) + "): " + String(sensorValue);
    mesh.sendSingle(storedGatewayId, msg);
  }
  // Schedule next transmission at a random interval between 1 and 5 seconds.
  taskSendSensorData.setInterval(random(TASK_SECOND, TASK_SECOND * 10));
});

//*************** Task: Send Health Report ********************
// This task gathers health metrics (RSSI and free heap) and sends them to the gateway.
Task taskSendHealth(TASK_SECOND * 20, TASK_FOREVER, []() {
  // Check if the stored gateway broadcast is stale.
  if (storedGatewayId != 0 && (millis() - lastGatewayReceivedTime) > gatewayBroadcastTimeout) {
    // Serial.printf("[%s-%d] Gateway broadcast timeout (health task). Clearing stored gateway ID.\n", deviceType.c_str(), deviceNumber);
    storedGatewayId = 0;
  }

  if (storedGatewayId != 0) {
    int rssi = WiFi.RSSI();       // Get signal strength in dBm
    int freeHeap = ESP.getFreeHeap();  // Get available heap memory
    String healthMsg = "Health from " + deviceType + "-" + String(deviceNumber) +
                       " | RSSI: " + String(rssi) +
                       " | FreeHeap: " + String(freeHeap);
    mesh.sendSingle(storedGatewayId, healthMsg);
    // Serial.printf("[%s-%d] Sent health report: %s\n", deviceType.c_str(), deviceNumber, healthMsg.c_str());
  } else {
    // Serial.printf("[%s-%d] Gateway ID unknown or stale. Waiting for gateway info (health report)...\n", deviceType.c_str(), deviceNumber);
  }
  // Schedule next health report at a random interval between 1 and 2 minutes.
  taskSendHealth.setInterval(random(TASK_SECOND, TASK_SECOND * 20));
});

//*************** Callback: Received Messages ********************
// When a message is received, if it's a gateway broadcast, update the stored gateway ID
// and refresh the timestamp. Otherwise, print the received message.
void receivedCallback(uint32_t from, String &msg) {
  // If the message is a gateway broadcast:
  if (msg.startsWith("GATEWAY:")) {
    uint32_t id = msg.substring(8).toInt();
    // Only update and print if the gateway ID is new.
    if (id != 0 && id != storedGatewayId) {
      storedGatewayId = id;
    }
    // Update the timestamp regardless of change.
    lastGatewayReceivedTime = millis();
  }
}

//*************** Callback: New Connection ********************
// When a new node connects, send it the current gateway ID directly.
void newConnectionCallback(uint32_t nodeId) {
  if (storedGatewayId != 0) {
    String msg = "GATEWAY:" + String(storedGatewayId);
    mesh.sendSingle(nodeId, msg);
  }
}

void changedConnectionCallback() {
}

void nodeTimeAdjustedCallback(int32_t offset) {
}

//*************** setup() *****************************
void setup() {
  Serial.begin(115200);
  
  // Set debug message types.
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  
  // Initialize the mesh network.
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  // Register callbacks.
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  // Add and enable tasks.
  userScheduler.addTask(taskSendSensorData);
  taskSendSensorData.enable();
  userScheduler.addTask(taskSendHealth);
  taskSendHealth.enable();
}

void loop() {
  // Continuously update the mesh network.
  mesh.update();
}