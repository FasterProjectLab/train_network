using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using WebSocketApp.Models;

namespace WebSocketApp.Services;

public class MessageHandler(ConnectionManager connectionManager)
{
    public async Task HandleAsync(string senderId, string rawMessage)
    {
        try
        {
            var options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
            var msg = JsonSerializer.Deserialize<WsMessage>(rawMessage, options);

            if (msg == null) return;

            if (msg.Type == "get-clients")
            {
                await SendUserList(senderId);
            } else if (!string.IsNullOrEmpty(msg.To))
            {
                await ForwardMessage(senderId, msg);
            }
        }
        catch (Exception ex) { Console.WriteLine($"[Error] {ex.Message}"); }
    }

    private async Task SendUserList(string clientId)
    {
        var socket = connectionManager.GetClient(clientId);
        if (socket == null) return;

        var response = new { type = "users-list", users = connectionManager.GetAllIds() };
        await SendJsonAsync(socket, response);
    }

    private async Task ForwardMessage(string senderId, WsMessage msg)
    {
        var targetSocket = connectionManager.GetClient(msg.To!);
        if (targetSocket != null && targetSocket.State == WebSocketState.Open)
        {
            var payload = new { from = senderId, type = msg.Type, data = msg.Data };
            await SendJsonAsync(targetSocket, payload);
        }
    }

    private static async Task SendJsonAsync(WebSocket socket, object data)
    {
        var json = JsonSerializer.Serialize(data);
        var bytes = Encoding.UTF8.GetBytes(json);
        await socket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private int _imageCounter = 0;
    private const int SaveInterval = 5;

    public async Task ForwardBinaryAsync(string senderId, byte[] data, int count)
    {
        try 
        {
            _imageCounter++;


            // 2. Relais vers React (toujours actif pour le temps réel)
            foreach (var targetId in connectionManager.GetAllIds())
            {
                if (targetId == senderId) continue;

                var targetSocket = connectionManager.GetClient(targetId);
                if (targetSocket != null && targetSocket.State == WebSocketState.Open)
                {
                    // On envoie le segment exact reçu pour ne pas envoyer de données vides
                    await targetSocket.SendAsync(
                        new ArraySegment<byte>(data, 0, count), 
                        WebSocketMessageType.Binary, 
                        true, 
                        CancellationToken.None);
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[Error] {ex.Message}");
        }
    }
}