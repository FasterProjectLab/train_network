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

            if (msg.Type == "get_clients")
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

        var response = new { type = "users", users = connectionManager.GetAllIds() };
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
}