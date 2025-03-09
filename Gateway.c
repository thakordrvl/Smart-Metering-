#include "painlessMesh.h"

//*************** Configuration *******************
#define MESH_PREFIX     "whateverYouLike"
#define MESH_PASSWORD   "somethingSneaky"
#define MESH_PORT       5555

// Define this macro to designate this node as the gateway.
#define GATEWAY

//*************** Global Objects and Scheduler ********************
Scheduler userScheduler;  // Task scheduler (no delay())
painlessMesh mesh;        // Mesh network object

//*************** Task: Gateway Broadcast ********************
// This task broadcasts the gateway node ID every 10 seconds.
Task taskGatewayBroadcast( TASK_SECOND * 10, TASK_FOREVER, []() {
  String msg = "GATEWAY:" + String(mesh.getNodeId());
  mesh.sendBroadcast(msg);
  // Serial.println("Gateway broadcast: " + msg); 
});

//*************** Callback Functions ********************

// Called when a message is received on the mesh.
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received on Gateway from %u: %s\n", from, msg.c_str());
  // Gateway can process received messages here (e.g., log sensor data, forward to cloud, etc.)
}

// Called when a new node connects.
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New connection on Gateway: nodeId = %u\n", nodeId);
}

// Called when connections change.
void changedConnectionCallback() {
  Serial.println("Connections changed");
}

// Called when the node's time is adjusted.
void nodeTimeAdjustedCallback(int32_t offset) {
  //Serial.printf("Time adjusted, offset = %d\n", offset);
}

//*************** setup() *****************************
void setup() {
  Serial.begin(115200);
  
  // Set debug message types.
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  
  // Designate this node as the gateway.
  #ifdef GATEWAY
    mesh.setRoot(true);
  #endif
  
  // Initialize the mesh network.
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  // Print the gateway node's ID to the Serial monitor.
  Serial.printf("Gateway Node ID: %u\n", mesh.getNodeId());
  
  // Register callbacks.
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  // Add and enable the gateway broadcast task (broadcast every 10 seconds).
  userScheduler.addTask(taskGatewayBroadcast);
  taskGatewayBroadcast.enable();
}

void loop() {
  // Continuously update the mesh network.
  mesh.update();
}