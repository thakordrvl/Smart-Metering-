package com.SmartMetering;

public class DataPayload {
    private String data;

    // Default constructor (required for JSON deserialization)
    public DataPayload() {
    }

    // Constructor with data parameter
    public DataPayload(String data) {
        this.data = data;
    }

    // Getter and setter
    public String getData() {
        return data;
    }

    public void setData(String data) {
        this.data = data;
    }
}
