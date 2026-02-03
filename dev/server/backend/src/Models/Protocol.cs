using System.Net.WebSockets;

namespace WebSocketApp.Models;

/// <summary>
/// Represents an active WebSocket connection.
/// Using a record ensures the session data is immutable and thread-safe.
/// </summary>
/// <param name="Id">The Client ID (e.g., Train-01 or Admin-PC).</param>
/// <param name="ConnectionId">The unique GUID for this specific socket instance.</param>
/// <param name="Socket">The underlying WebSocket object.</param>
/// <param name="Type">The category of client (train vs user).</param>
public record ClientSession(
    string Id, 
    string ConnectionId, 
    WebSocket Socket, 
    string Type
);

/// <summary>
/// Centralized protocol definitions to ensure consistency between 
/// the ESP32 firmware, C# Backend, and React Frontend.
/// </summary>
public static class ProtocolConstants
{
    // Client Types
    public const string TypeTrain = "train";
    public const string TypeUser = "user";

    // Common Targets
    public const string TargetServer = "server";
    public const string TargetBroadcast = "broadcast";

    // Action/Message Types
    public const string ActionSystem = "system";          // Reboot, OTA, etc.
    public const string ActionMotor = "motor";            // Speed and Direction
    public const string ActionLight = "light";            // Headlights/Flash
    public const string ActionConnection = "conn";        // Initial Handshake
    public const string ActionAckConnection = "conn_ack"; // Handshake Response
    public const string ActionSubscribe = "subscribe";    // Start Telemetry/Video
    public const string ActionUnSubscribe = "unsubscribe";// Stop Telemetry/Video
    public const string ActionGetTains = "get_trains";    // Discovery request
    public const string ActionPing = "ping";              // Keep-alive
    public const string ActionCameraControl = "camera_control"; // Resolution/Quality
}