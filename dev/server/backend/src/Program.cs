using System.Net.WebSockets;
using System.Text;
using System.Threading.Channels;
using WebSocketApp.Models;
using WebSocketApp.Services;

var builder = WebApplication.CreateBuilder(args);

// --- SERVICE REGISTRATION (Dependency Injection) ---
// We use Singletons because these services must maintain state across all concurrent connections.
builder.Services.AddSingleton<ConnectionManager>();
builder.Services.AddSingleton<MessageHandler>();
builder.Services.AddSingleton<VideoBroadcastService>(); 
// UdpReceiverService runs in the background as a worker to catch raw video packets.
builder.Services.AddHostedService<UdpReceiverService>(); 

builder.WebHost.ConfigureKestrel(options => { options.Limits.MaxRequestBodySize = 100_000; });

// --- CORS POLICY ---
// Essential for allowing your React frontend (localhost:3000) to talk to this backend.
builder.Services.AddCors(options => {
    options.AddPolicy("AllowAll", policy => {
        policy.AllowAnyOrigin().AllowAnyMethod().AllowAnyHeader();
    });
});

var app = builder.Build();

app.UseCors("AllowAll");
app.UseWebSockets(new WebSocketOptions { KeepAliveInterval = TimeSpan.FromSeconds(5) });

// --- ROUTE 1: CONTROL WEB SOCKETS ---
// URL Pattern: ws://server/ws/{id}?type=train|user
app.Map("/ws/{id}", async (string id, HttpContext context, ConnectionManager mgr, MessageHandler handler) =>
{
    if (!context.WebSockets.IsWebSocketRequest) return;

    string type = context.Request.Query["type"].ToString().ToLower();
    if (type != ProtocolConstants.TypeTrain && type != ProtocolConstants.TypeUser) return;

    // Preventive cleanup: if a train reconnects, kill ghost sessions
    if (type == ProtocolConstants.TypeTrain) await mgr.ClearOldSessionsForTrain(id);

    using var webSocket = await context.WebSockets.AcceptWebSocketAsync();
    string connectionId = Guid.NewGuid().ToString();
    
    mgr.AddClient(id, connectionId, webSocket, type);
    Console.WriteLine($"[CONNECTED] {id} [{connectionId}] ({type})");

    if (type == ProtocolConstants.TypeTrain) {
        await handler.NotifyTrainStatusAsync(connectionId, id, "connected");
    }

    var buffer = new byte[1024 * 64]; 

    try
    {
        while (webSocket.State == WebSocketState.Open)
        {
            // Fragmented message reassembly loop
            using var ms = new MemoryStream();
            WebSocketReceiveResult result;
            do
            {
                // Safety timeout to prevent hung connections
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                result = await webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), cts.Token);
                ms.Write(buffer, 0, result.Count);
            } while (!result.EndOfMessage);

            if (result.MessageType == WebSocketMessageType.Close) break;

            if (result.MessageType == WebSocketMessageType.Text)
            {
                var text = Encoding.UTF8.GetString(ms.ToArray());
                await handler.HandleAsync(connectionId, id, text);
            }
        }
    }
    catch (WebSocketException)
    {
        Console.WriteLine($"[WS DISCONNECT] {id} disconnected abruptly.");
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[WS ERROR] {id}: {ex.Message}");
    }
    finally
    {
        // 1. Get the list of topics the user was watching BEFORE removing them
        var userInterests = mgr.GetUserTopics(id);

        // 2. Remove the client from the manager
        await mgr.RemoveClient(connectionId);
        Console.WriteLine($"[DISCONNECTED] {id} [{connectionId}]");

        if (type == ProtocolConstants.TypeTrain) 
        {
            await handler.NotifyTrainStatusAsync(connectionId, id, "disconnected");
        } 
        else if (type == ProtocolConstants.TypeUser) 
        {
            // 3. For each topic (e.g. "train01:camera"), check if we can stop the hardware
            foreach (var topic in userInterests)
            {
                string[] parts = topic.Split(':'); // This creates the string array
                if (parts.Length == 2) 
                {
                    // Corrected method name: StopResourceIfNoSubscribers
                    await handler.StopResourceIfNoSubscribers(parts[0], parts[1]);
                }
            }
        }
    }
});

// --- ROUTE 2: MJPEG VIDEO STREAMING ---
// URL Pattern: http://server/video/{trainId}?userId={userId}
// This acts as an MJPEG Proxy between the internal UDP stream and the Browser <img> tag.
app.MapGet("/video/{trainId}", async (string trainId, string userId, HttpContext context, VideoBroadcastService broadcaster, ConnectionManager mgr, CancellationToken ct) => {
    
    // Security: Only allow video if the user is currently subscribed via WebSocket
    var subscribers = mgr.GetSubscribers(trainId, "camera");
    if (!subscribers.Contains(userId))
    {
        context.Response.StatusCode = StatusCodes.Status403Forbidden;
        return;
    }

    // Set standard MJPEG headers
    context.Response.ContentType = "multipart/x-mixed-replace; boundary=--frame";
    var channel = broadcaster.Subscribe(trainId);

    try
    {
        // Read frames from the Channel as they arrive from UdpReceiverService
        await foreach (var frame in channel.Reader.ReadAllAsync(ct))
        {
            await context.Response.WriteAsync("--frame\r\n", ct);
            await context.Response.WriteAsync("Content-Type: image/jpeg\r\n", ct);
            await context.Response.WriteAsync($"Content-Length: {frame.Length}\r\n\r\n", ct);
            await context.Response.Body.WriteAsync(frame, ct);
            await context.Response.WriteAsync("\r\n", ct);
            await context.Response.Body.FlushAsync(ct);
        }
    }
    catch (OperationCanceledException) { /* Browser closed tab or stopped stream */ }
    catch (Exception ex) { Console.WriteLine($"[STREAM ERROR] {ex.Message}"); }
});

app.Run();