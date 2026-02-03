using System.Net;
using System.Net.Sockets;
using System.Buffers.Binary;
using System.Collections.Concurrent;

namespace WebSocketApp.Services;

/// <summary>
/// Background service responsible for consuming raw RTP/UDP video packets from ESP32-CAM devices.
/// It handles frame reconstruction, SSRC-based routing, and dynamic latency calculation.
/// </summary>
public class UdpReceiverService : BackgroundService
{
    private readonly VideoBroadcastService _broadcaster;
    private readonly ConnectionManager _mgr;

    // Maps SSRC to the clock offset (Server Time - ESP32 Boot Time) for latency tracking
    private readonly ConcurrentDictionary<uint, long> _clockOffsets = new();

    // Tracks the last received RTP timestamp per SSRC to detect hardware reboots
    private readonly ConcurrentDictionary<uint, uint> _lastRtpTimestamps = new();

    public UdpReceiverService(VideoBroadcastService broadcaster, ConnectionManager mgr)
    {
        _broadcaster = broadcaster;
        _mgr = mgr;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var udpClient = new UdpClient();
        
        // Allow multiple services to bind to the same port if needed (standard for high-load UDP)
        udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        udpClient.Client.Bind(new IPEndPoint(IPAddress.Any, 5004));
        
        Console.WriteLine("[UDP] Listening for video packets on port 5004...");

        // Buffer used to reassemble fragmented JPEG packets into complete frames
        var frameBuffers = new ConcurrentDictionary<uint, MemoryStream>();

        while (!stoppingToken.IsCancellationRequested)
        {
            try 
            {
                var result = await udpClient.ReceiveAsync(stoppingToken);
                byte[] data = result.Buffer;

                // Minimum RTP header size check
                if (data.Length < 20) continue;

                // 1. Extract SSRC from the RTP header (Bytes 8-11)
                uint ssrc = BinaryPrimitives.ReadUInt32BigEndian(data.AsSpan(8, 4));

                // 2. Append payload data (skipping the 20-byte custom RTP header) to the stream
                var ms = frameBuffers.GetOrAdd(ssrc, _ => new MemoryStream());
                await ms.WriteAsync(data.AsMemory(20), stoppingToken);

                // 3. Check for the 'Marker' bit (indicates the end of a JPEG frame)
                if ((data[1] & 0x80) != 0)
                { 
                    uint rtpTimestamp = BinaryPrimitives.ReadUInt32BigEndian(data.AsSpan(4, 4));
                    long serverNow = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

                    #region Latency & Reboot Logic

                    // Detect if the ESP32 restarted (Timestamp usually resets to 0 or wraps around)
                    if (_lastRtpTimestamps.TryGetValue(ssrc, out uint lastTS))
                    {
                        if (rtpTimestamp < lastTS - 1000) 
                        {
                            Console.WriteLine($"[SSRC 0x{ssrc:X8}] Hardware reboot detected. Resetting clock offset.");
                            _clockOffsets.TryRemove(ssrc, out _);
                        }
                    }
                    _lastRtpTimestamps[ssrc] = rtpTimestamp;

                    // Calculate or retrieve the time difference between server and device
                    long currentOffset = _clockOffsets.GetOrAdd(ssrc, _ => serverNow - rtpTimestamp);

                    // Calculate current network/processing latency
                    long latency = serverNow - (rtpTimestamp + currentOffset);

                    // Dynamic Clock Correction: If latency is negative, the device clock is drifting 
                    // ahead or network jitter occurred. We adjust the offset to normalize it.
                    if (latency < 0) 
                    {
                        _clockOffsets.AddOrUpdate(ssrc, 
                            _ => serverNow - rtpTimestamp, 
                            (_, oldOffset) => oldOffset + latency);
                        latency = 0;
                    }

                    #endregion

                    // 4. Dispatch the completed frame
                    byte[] frame = ms.ToArray();
                    ms.SetLength(0); // Clear buffer for the next frame

                    // Route the frame to the specific Train ID associated with this SSRC
                    string? trainId = _mgr.GetTrainIdBySsrc(ssrc); 
                    if (trainId != null) 
                    {
                        await _broadcaster.DispatchFrame(trainId, frame);
                    }
                }
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                Console.WriteLine($"[UDP ERROR] {ex.Message}");
            }
        }
    }
}