// src/models/Protocol.ts

/**
 * @brief Constants defining the communication protocol between 
 * the Controller (React), the Dispatch Server (Go/Node), and the Trains (ESP32).
 */
export const ProtocolConstants = {
  // Client Classifications
  TypeUser: "user",
  TypeTrain: "train",

  // Reserved Routing Targets
  TargetServer: "server",

  // Connection Lifecycle Actions
  ActionConn: "conn",
  ActionConnAck: "conn_ack",

  // Device Control Actions
  ActionSetSpeed: "motor",      // Used for MotorControl logic
  ActionSystem: "system",       // Used for SystemActions (Reboot, OTA, etc.)
  ActionLog: "log",             // Used for console/terminal feedback
  
  // Subscription Management (Pub/Sub)
  ActionSubscibe: "subscribe",
  ActionUnsubscribe: "unsubscribe",
};

/**
 * @interface MessageEnvelope
 * @brief Standard JSON structure for all messages flowing through the WebSocket.
 * Matches the 'send_ws_envelope' structure used in the ESP32 firmware.
 */
export interface MessageEnvelope {
  type: string;        // The action or command type (from ProtocolConstants)
  source: string;      // Unique ID of the sender (MAC address or Controller ID)
  target: string;      // Unique ID of the recipient or "server"
  payload?: any;       // Optional data object specific to the command
  timestamp?: number;  // Unix timestamp (milliseconds) for synchronization/logging
}