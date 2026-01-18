var builder = WebApplication.CreateBuilder(args);
var app = builder.Build();

app.UseWebSockets();

app.Map("/ws", async context =>
{
    if (context.WebSockets.IsWebSocketRequest)
    {
        using var webSocket = await context.WebSockets.AcceptWebSocketAsync();
        var buffer = new byte[1024 * 4];
        
        while (webSocket.State == System.Net.WebSockets.WebSocketState.Open)
        {
            var result = await webSocket.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);
            if (result.MessageType == System.Net.WebSockets.WebSocketMessageType.Close)
            {
                await webSocket.CloseAsync(System.Net.WebSockets.WebSocketCloseStatus.NormalClosure, "Closing", CancellationToken.None);
            }
            else
            {
                // Echo : on renvoie ce qu'on reçoit
                await webSocket.SendAsync(new ArraySegment<byte>(buffer, 0, result.Count), result.MessageType, result.EndOfMessage, CancellationToken.None);
            }
        }
    }
    else
    {
        context.Response.StatusCode = StatusCodes.Status400BadRequest;
    }
});

app.Run();