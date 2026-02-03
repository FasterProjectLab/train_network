using System.Collections.Concurrent;
using System.Threading.Channels;

namespace WebSocketApp.Services;

/// <summary>
/// High-performance video distribution service.
/// Uses System.Threading.Channels to provide non-blocking frame delivery 
/// from the UDP receiver to multiple connected WebSocket clients.
/// </summary>
public class VideoBroadcastService
{
    // Holds the list of active memory channels (one per web viewer) grouped by Train ID
    private readonly ConcurrentDictionary<string, ConcurrentBag<Channel<byte[]>>> _subscribers = new();
    
    // Cache of the most recent frame for each train to provide instant feedback upon subscription
    private readonly ConcurrentDictionary<string, byte[]> _lastFrames = new();

    /// <summary>
    /// Creates a new subscription channel for a specific train.
    /// </summary>
    /// <param name="trainId">The unique ID of the train to watch.</param>
    /// <returns>A Bounded Channel that will receive JPEG byte arrays.</returns>
    public Channel<byte[]> Subscribe(string trainId)
    {
        // Define a bounded channel to prevent memory leaks if a client is too slow.
        // If the buffer (20 frames) fills up, we drop the oldest frames to maintain real-time low latency.
        var options = new BoundedChannelOptions(20) 
        { 
            FullMode = BoundedChannelFullMode.DropOldest,
            SingleReader = true, // Each subscription is read by exactly one WebSocket task
            SingleWriter = false  // Multiple sources could theoretically push frames
        };

        var channel = Channel.CreateBounded<byte[]>(options);
            
        // If we already have a frame in cache, send it immediately so the user doesn't see a black screen
        if (_lastFrames.TryGetValue(trainId, out var lastFrame))
        {
            channel.Writer.TryWrite(lastFrame);
        }
        
        // Register the channel in our subscriber dictionary
        _subscribers.AddOrUpdate(trainId, 
            _ => new ConcurrentBag<Channel<byte[]>> { channel }, 
            (_, bag) => { 
                bag.Add(channel); 
                return bag; 
            });
            
        return channel;
    }

    /// <summary>
    /// Pushes a newly reconstructed JPEG frame to all active subscribers of a train.
    /// Called by the UdpReceiverService once a full MJPEG frame is assembled.
    /// </summary>
    /// <param name="trainId">Source train ID.</param>
    /// <param name="frame">Raw JPEG byte data.</param>
    public async Task DispatchFrame(string trainId, byte[] frame)
    {
        // Update the "Last Known Frame" cache
        _lastFrames[trainId] = frame;

        // Broadcast to all channels currently watching this train
        if (_subscribers.TryGetValue(trainId, out var channels))
        {
            foreach (var channel in channels)
            {
                // Note: Using WriteAsync ensures we don't block the UDP thread 
                // if a channel's buffer is momentarily full.
                await channel.Writer.WriteAsync(frame);
            }
        }
    }
    
    /// <summary>
    /// Optional: Logic to remove closed channels would typically be added here 
    /// to clean up the ConcurrentBag when a WebSocket disconnects.
    /// </summary>
    public void Unsubscribe(string trainId, Channel<byte[]> channel)
    {
        // Implementation for cleaning up the bag...
        channel.Writer.TryComplete();
    }
}