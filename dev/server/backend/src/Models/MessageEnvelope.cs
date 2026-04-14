using System.Text.Json.Serialization;

namespace WebSocketApp.Models;

/// <summary>
/// The standard communication wrapper for all messages in the system.
/// This matches the TypeScript 'MessageEnvelope' interface.
/// </summary>
public class MessageEnvelope
{
    [JsonPropertyName("type")]
    public string Type { get; set; } = string.Empty;

    [JsonPropertyName("tag")]
    public string Tag { get; set; } = string.Empty;

    [JsonPropertyName("source")]
    public string Source { get; set; } = string.Empty;

    [JsonPropertyName("target")]
    public string Target { get; set; } = string.Empty;

    [JsonPropertyName("payload")]
    public object? Payload { get; set; }

    [JsonPropertyName("timestamp")]
    public long Timestamp { get; set; } = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
}

/// <summary>
/// Represents the hardware health and resource metrics sent by the ESP32.
/// </summary>
public class TelemetryPayload
{
    [JsonPropertyName("ram_free")]
    public long RamFree { get; set; }

    [JsonPropertyName("ram_min")]
    public long RamMin { get; set; }

    [JsonPropertyName("psram_free")]
    public long PsramFree { get; set; }

    [JsonPropertyName("rssi")]
    public int Rssi { get; set; }

    [JsonPropertyName("uptime")]
    public long Uptime { get; set; }

    [JsonPropertyName("stack_mon")]
    public int StreamStackMin { get; set; }
}