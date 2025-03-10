// server.js
const express = require('express');
const bodyParser = require('body-parser');
const app = express();

app.use(bodyParser.json());  // Parse JSON payloads

// Global array to store sensor data
const dataLog = [];

// Endpoint to receive data via POST
app.post('/data', (req, res) => {
  if (req.body && req.body.data) {
    dataLog.push(req.body);
    console.log('Received data:', req.body);
    res.status(200).send('OK');
  } else {
    res.status(400).send('No data received');
  }
});

// Endpoint to retrieve all data via GET
app.get('/data', (req, res) => {
  res.json(dataLog);
});

// Start the server on port 5000
const PORT = 5000;
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
