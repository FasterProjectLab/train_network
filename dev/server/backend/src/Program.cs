using WebSocketApp.Services;
using System.Net.WebSockets;
using System.Text;

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
            using var ms = new MemoryStream();
            WebSocketReceiveResult result;
            
            // On boucle pour reconstruire l'image complète
            do
            {
                result = await webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);
                ms.Write(buffer, 0, result.Count);
            } 
            while (!result.EndOfMessage);

            if (result.MessageType == WebSocketMessageType.Close) break;

            else if (result.MessageType == WebSocketMessageType.Binary)
            {
                byte[] frame = ms.ToArray();

                // (Optionnel) sécurité taille frame
                if (frame.Length > 400_000)
                    return;

                // Relais du flux vers les autres clients (admins)
                foreach (var client in mgr.GetAllExcept(id))
                {
                    if (client.State == WebSocketState.Open)
                    {
                        await client.SendAsync(
                            new ArraySegment<byte>(frame),
                            WebSocketMessageType.Binary,
                            true,
                            CancellationToken.None
                        );
                    }
                }
            }

            else if (result.MessageType == WebSocketMessageType.Text){
                string textContent = Encoding.UTF8.GetString(ms.ToArray());
                await handler.HandleAsync(id, textContent);
            }
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