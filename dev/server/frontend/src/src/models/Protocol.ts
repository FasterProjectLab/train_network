// src/models/Protocol.ts

/**
 * @brief Constants defining the communication protocol between 
 * the Controller (React), the Dispatch Server (Go/Node), and the Trains (ESP32).
 */
export const ProtocolConstants = {
  // Client Classifications
  TypeUser: "user",
  TypeTrain: "train",
  TypeTrackController: "track_controller",

  // Reserved Routing Targets
  TargetServer: "server",

  // Connection Lifecycle Actions
  ActionWelcom: "welcom",
  ActionConnAck: "ack_welcom",
  ActionGoodbye: "goodbye",

  ActionGetTrains: "get_trains",
  ActionGetTrackControllers: "get_track_controllers",
  ActionGetTurnouts: "get_turnouts",
  ActionSetTurnouts: "set_turnouts",
  ActionTrainStatus: "train_status",
  ActionGetTrainStatus: "get_train_status",
  ActionLight: "light",
  ActionMotor: "motor",

  // Device Control Actions
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
  tag?: string;
  source: string;      // Unique ID of the sender (MAC address or Controller ID)
  target: string;      // Unique ID of the recipient or "server"
  payload?: any;       // Optional data object specific to the command
  timestamp?: number;  // Unix timestamp (milliseconds) for synchronization/logging
}

export interface TurnoutDevice {
  id: string;
  position: 'normal' | 'divergent';
}

export interface TrackController {
  id: string;
  sessionId: string;
  status: string;
  turnouts: TurnoutDevice[];
}

export interface MotorStatus {
  speed: number;
  dir: 'fwd' | 'rev';
}

export interface TrainStatus {
  light: boolean;
  motor: MotorStatus;
  cameraActive: boolean;
  telemetryActive: boolean;
}

export interface Train {
  id: string;             
  sessionId: string;      
  status: TrainStatus;    
  lastUpdated: number;   
}