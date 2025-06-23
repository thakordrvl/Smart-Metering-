# Smart Metering using Mesh Network

This repository contains the full development cycle of a **smart metering system** built on a **mesh network architecture**. The system is designed for **energy-efficient**, **secure**, and **scalable** data transmission from distributed smart meter nodes to a central gateway, using **multi-hop mesh communication** and **optional encryption**.

## Project Overview

The smart metering solution leverages a custom mesh network protocol using ESP-based microcontrollers. It supports:

* **Multiple hub nodes** to scale the network.
* **Energy-aware message routing** to reduce power consumption.
* **Encryption support** for secure transmission of metering data.
* **Web dashboard** for real-time data visualization.

This project was developed in iterative phases, evolving from a basic mesh to a production-ready, encrypted, multi-hub smart metering solution.

---

## Architecture

### 🔧 Core Components

* **Smart Meter Nodes**: Devices that collect energy usage data and transmit via mesh.
* **Hub Nodes**: Act as intermediaries to relay data efficiently across the network.
* **Gateway Node**: The final sink that collects data and forwards it to a backend or dashboard.
* **Website Dashboard**: A real-time web interface to monitor readings and node status.

### 🌐 Multi-Hub Mesh Network

* Fully functional **multi-hub mesh** routing based on hop counts.
* Implements **round-robin polling** to prevent data congestion.
* Each hub is assigned a **localHubId** to maintain organized message routing.
* Buffered message queues to ensure reliability even under variable latency.

---

## Features

### ✅ Energy-Efficient Communication

* Optimized packet scheduling to reduce collisions and idle listening.
* Uses selective broadcasting and hop-count updates to conserve energy.

### 🔐 Optional Encryption

* Public-key encryption (ECC/RSA) for secure transmission from meter to gateway.
* Lightweight crypto compatible with both **ESP32** and **ESP8266** nodes.

### 🔁 Multi-Hub Support

* Network auto-adjusts to available hub paths.
* Data can route dynamically depending on hub availability and proximity.

### 📊 Smart Dashboard

* Built-in web interface to visualize:

  * Real-time power consumption
  * Node status (online/offline)
  * Network topology

---

## Repository Contents

* **Traditional Mesh**
  Early development phase with basic hub and gateway integration. Contains initial tests but has unresolved bugs and incomplete routing logic.

* **Energy Efficient Mesh with Encryption**
  Introduced major code restructuring and energy-saving communication strategies. Encryption was implemented. Multi-hub logic was not yet integrated at this point.

* **Energy Efficient Mesh with Multiple Hub Nodes**
  Final, stable version with full support for multiple hub nodes, robust message buffering, hop-based routing, and round-robin polling. All features tested and verified. Considered the production-ready version.

* **SmartMetering**
  Demonstration-ready version integrating node firmware, hubs, gateway logic, and a real-time dashboard. Successfully used for a full working demo of the end-to-end smart metering system.

---

## Status

* ✅ All major components implemented and tested.
* ✅ Multi-hub mesh routing working stably.
* ✅ Dashboard functional and responsive.
