using WebSocketApp.Services;
using System.Net.WebSockets;

var builder = WebApplication.CreateBuilder(args);
builder.Services.AddSingleton<ConnectionManager>();
builder.Services.AddSingleton<MessageHandler>();

var app = builder.Build();
app.UseWebSockets();

app.Map("/ws/{id}", async (string id, HttpContext context, ConnectionManager mgr, MessageHandler handler) =>
{
    if (!context.WebSockets.IsWebSocketRequest) return;

    // Configure les options du WebSocket
    var options = new WebSocketAcceptContext {
        // Le serveur enverra un "Ping" toutes les 30 secondes
        KeepAliveInterval = TimeSpan.FromSeconds(30) 
    };

    using var webSocket = await context.WebSockets.AcceptWebSocketAsync(options);

    mgr.AddClient(id, webSocket);

    string socketId = webSocket.GetHashCode().ToString("X");

    Console.WriteLine($"[+] {id} [Socket:{socketId}] connecté");

    var buffer = new byte[1024 * 4];
    try
    {
        while (webSocket.State == System.Net.WebSockets.WebSocketState.Open)
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
    
            var result = await webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), cts.Token);
            
            if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Close) break;

            var message = System.Text.Encoding.UTF8.GetString(buffer, 0, result.Count);
            await handler.HandleAsync(id, message);
        }
    }
    catch (System.Net.WebSockets.WebSocketException ex)
    {
        Console.WriteLine($"[!] Erreur réseau pour {id} [Socket:{socketId}] : {ex.Message}");
    }
    catch (Exception ex)
    {
        Console.WriteLine($"[!] Erreur pour {id} [Socket:{socketId}] : {ex.Message}");
        
    }
    finally
    {
        await mgr.RemoveClient(id, webSocket);
        Console.WriteLine($"[-] {id} [Socket:{socketId}] déconnecté");
        
    }
});

app.Run();