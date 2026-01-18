using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Collections.Concurrent;

var builder = WebApplication.CreateBuilder(args);
var app = builder.Build();

app.UseWebSockets();

var clients = new ConcurrentDictionary<string, WebSocket>();

app.Map("/ws/{id}", async (string id, HttpContext context) =>
{
    if (context.WebSockets.IsWebSocketRequest)
    {
        using var webSocket = await context.WebSockets.AcceptWebSocketAsync();
        
        // Enregistrement du client
        clients[id] = webSocket;
        Console.WriteLine($"Client '{id}' connecté. Total: {clients.Count}");

        try
        {
            var buffer = new byte[1024 * 4];
            while (webSocket.State == WebSocketState.Open)
            {
                var result = await webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);

                if (result.MessageType == WebSocketMessageType.Text)
                {
                    var messageRaw = Encoding.UTF8.GetString(buffer, 0, result.Count);
                    
                    try 
                    {
                        // Analyse du JSON
                        using var doc = JsonDocument.Parse(messageRaw);
                        var root = doc.RootElement;
                        
                        string targetId = root.GetProperty("to").GetString();
                        var data = root.GetProperty("data").GetRawText();

                        if (clients.TryGetValue(targetId, out var targetSocket) && targetSocket.State == WebSocketState.Open)
                        {
                            var responseBytes = Encoding.UTF8.GetBytes(data);
                            await targetSocket.SendAsync(new ArraySegment<byte>(responseBytes), WebSocketMessageType.Text, true, CancellationToken.None);
                            Console.WriteLine($"Message de '{id}' vers '{targetId}' transféré.");
                        }
                        else
                        {
                            Console.WriteLine($"Destinataire '{targetId}' non trouvé.");
                        }
                    }
                    catch (Exception ex) { Console.WriteLine($"Erreur format JSON: {ex.Message}"); }
                }
                else if (result.MessageType == WebSocketMessageType.Close)
                {
                    break;
                }
            }
        }
        finally
        {
            // Nettoyage de la RAM à la déconnexion
            clients.TryRemove(id, out _);
            Console.WriteLine($"Client '{id}' déconnecté.");
        }
    }
    else
    {
        context.Response.StatusCode = StatusCodes.Status400BadRequest;
    }
});

app.Run();