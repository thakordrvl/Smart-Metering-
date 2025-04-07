package com.SmartMetering;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.time.LocalDateTime;
import com.SmartMetering.MeshData;

@RestController
@RequestMapping("/data")
public class DataController {

    @Autowired
    private MeshDataRepository repository;

    @Autowired
    private SimpMessagingTemplate messagingTemplate;

    // POST endpoint to receive data
    @PostMapping
    public String receiveData(@RequestBody DataPayload payload) {
        if (payload != null && payload.getData() != null) {
            MeshData record = new MeshData(payload.getData(), LocalDateTime.now());
            repository.save(record);
            System.out.println("Received data: " + payload.getData());
            // Broadcast new data to WebSocket subscribers
            messagingTemplate.convertAndSend("/topic/meshdata", payload.getData());
            return "OK";
        } else {
            return "Bad Request";
        }
    }

    // GET endpoint to retrieve all stored data
    @GetMapping
    public List<MeshData> getData() {
        return (List<MeshData>) repository.findAll();
    }
}




