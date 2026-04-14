using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using WebSocketApp.Models;

namespace WebSocketApp.Services;

/// <summary>
/// Orchestrates message routing and protocol logic.
/// Acts as the bridge between Controllers (React), Trains (ESP32), and the Connection Manager.
/// </summary>
public class MessageHandler
{
    private readonly ConnectionManager _mgr;
    private readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions 
    { 
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase 
    };

    public MessageHandler(ConnectionManager mgr)
    {
        _mgr = mgr;
    }

    /// <summary>
    /// Entry point for all incoming WebSocket text messages.
    /// Routes messages based on the 'Target' property in the envelope.
    /// </summary>
    public async Task HandleAsync(string sessionId, string senderId, string json)
    {
        Console.WriteLine($"[WS RECV] From: {senderId} | Raw: {json}");

        var envelope = JsonSerializer.Deserialize<MessageEnvelope>(json, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });
        if (envelope == null) return;

        // Route 1: Broadcast to all interested subscribers
        if (envelope.Target == ProtocolConstants.TargetBroadcast)
        {
            await BroadcastToSubscribersAsync(envelope.Source, envelope.Type, json);
        }
        // Route 2: Internal command intended for the Server logic
        else if (envelope.Target == ProtocolConstants.TargetServer)
        {
            await ProcessInternalCommand(sessionId, senderId, envelope);
        }
        // Route 3: Peer-to-Peer command (usually Controller -> Train)
        else
        {
            await ProcessExternalCommand(senderId, envelope);
        }
    }

    /// <summary>
    /// Sends a message to everyone subscribed to a specific resource (e.g., "train01:camera").
    /// </summary>
    private async Task BroadcastToSubscribersAsync(string trainId, string tag, string json)
    {
        var userIds = _mgr.GetSubscribers(trainId, tag);
        foreach (var userId in userIds)
        {
            await SendToTargetAsync(userId, json);
        }
    }

    #region Transmission Helpers

    /// <summary>
    /// Sends a text message to all active sessions of a specific Client ID.
    /// </summary>
    public async Task SendToTargetAsync(string targetId, string json)
    {
        var sessions = _mgr.GetSessions(targetId);
        var bytes = Encoding.UTF8.GetBytes(json);
        var segment = new ArraySegment<byte>(bytes);

        foreach (var session in sessions)
        {
            if (session.Socket.State == WebSocketState.Open)
            {
                try { await session.Socket.SendAsync(segment, WebSocketMessageType.Text, true, CancellationToken.None); }
                catch { /* Handle/Log socket failure */ }
            }
        }
    }

    /// <summary>
    /// Overload for sending raw binary data (e.g., Camera JPEGs) to a specific target.
    /// </summary>
    public async Task SendToTargetAsync(string targetId, byte[] data, WebSocketMessageType type = WebSocketMessageType.Binary)
    {
        var sessions = _mgr.GetSessions(targetId);
        var segment = new ArraySegment<byte>(data);

        foreach (var session in sessions)
        {
            if (session.Socket.State == WebSocketState.Open)
            {
                try { await session.Socket.SendAsync(segment, type, true, CancellationToken.None); }
                catch (Exception ex) { Console.WriteLine($"[SEND ERROR] {targetId}: {ex.Message}"); }
            }
        }
    }

    #endregion

    /// <summary>
    /// Handles logic for Controller-to-Train commands.
    /// Manages hardware resource activation (starting/stopping camera based on viewership).
    /// </summary>
    private async Task ProcessExternalCommand(string senderId, MessageEnvelope env)
    {
        switch (env.Type)
        {
            case ProtocolConstants.ActionSubscribe: 
                string? tag = env.Tag?.ToString()?.ToLower();
                if (string.IsNullOrEmpty(tag)) return;
                            
                var alreadyHasViewers = _mgr.GetSubscribers(env.Target, tag).Any();
                _mgr.Subscribe(senderId, env.Target, tag);

                // Optimization: Tell the train to start its hardware stream ONLY if this is the first subscriber
                if (!alreadyHasViewers)
                {
                    await SendControlToTrain(env.Target, $"{tag}_start", env.Payload);
                }
                break;

            case ProtocolConstants.ActionUnSubscribe:
                string? unSubTag = env.Payload?.ToString()?.ToLower();
                if (string.IsNullOrEmpty(unSubTag)) return;

                _mgr.Unsubscribe(senderId, env.Target, unSubTag);
                
                // Optimization: Tell the train to stop hardware stream if NO ONE is left watching
                if (!_mgr.GetSubscribers(env.Target, unSubTag).Any()) 
                {
                    await SendControlToTrain(env.Target, $"{unSubTag}_stop");
                }
                break;

            // Forward hardware controls directly to the target device
            case ProtocolConstants.ActionCameraControl:
            case ProtocolConstants.ActionMotor:
            case ProtocolConstants.ActionLight:
            case ProtocolConstants.ActionSystem:
                await SendControlToTrain(env.Target, env.Type, env.Payload);
                break;
        }
    }

    /// <summary>
    /// Wraps an action into a Protocol Envelope and dispatches it to a train.
    /// </summary>
    public async Task SendControlToTrain(string trainId, string actionType, object? payload = null)
    {
        var cmd = new MessageEnvelope
        {
            Type = actionType,
            Source = ProtocolConstants.TargetServer,
            Target = trainId,
            Payload = payload
        };
        await SendToTargetAsync(trainId, JsonSerializer.Serialize(cmd, _jsonOptions));
    }

    /// <summary>
    /// Handles requests meant for the server (Connection handshake, ping, train discovery).
    /// </summary>
    private async Task ProcessInternalCommand(string sessionId, string senderId, MessageEnvelope env)
    {
        switch (env.Type)
        {
            case ProtocolConstants.ActionConnection:
                // Handle SSRC registration if provided (used for mapping media streams)
                if (env.Payload is JsonElement payloadElement && payloadElement.TryGetProperty("ssrc", out var ssrcProp))
                {
                    _mgr.RegisterSessionSsrc(sessionId, ssrcProp.GetUInt32());
                }
                await SendActiveTrainsListAsync(senderId);
                break;

            case ProtocolConstants.ActionGetTains:
                await SendActiveTrainsListAsync(senderId);
                break;
            
            case ProtocolConstants.ActionPing:
                // Pings are handled silently to keep the socket alive
                break;
        }
    }

    /// <summary>
    /// Broadcasts a status change (Connect/Disconnect) to all connected UI users.
    /// </summary>
    public async Task NotifyTrainStatusAsync(string trainSessionId, string trainId, string status)
    {
        var message = new MessageEnvelope
        {
            Type = "train_status_change",
            Source = ProtocolConstants.TargetServer,
            Target = ProtocolConstants.TargetBroadcast,
            Payload = new { trainId = trainId, status = status, sessionId = trainSessionId }
        };

        string json = JsonSerializer.Serialize(message, _jsonOptions);
        foreach (var userId in _mgr.GetAllUserIds()) 
        {
            await SendToTargetAsync(userId, json);
        }
    }

    private async Task SendActiveTrainsListAsync(string targetId)
    {
        var response = new MessageEnvelope
        {
            Type = ProtocolConstants.ActionAckConnection,
            Source = ProtocolConstants.TargetServer,
            Target = targetId,
            Payload = new { active_trains = _mgr.GetActiveTrainSessions() }
        };
        
        await SendToTargetAsync(targetId, JsonSerializer.Serialize(response, _jsonOptions));
    }

    public async Task StopResourceIfNoSubscribers(string trainId, string tag)
    {
        var hasSubscribers = _mgr.GetSubscribers(trainId, tag).Any();

        if (!hasSubscribers)
        {
            Console.WriteLine($"[AUTO-STOP] No more subscribers for '{tag}' on {trainId}. Sending stop command.");
            await SendControlToTrain(trainId, $"{tag}_stop");
        }
    }
}