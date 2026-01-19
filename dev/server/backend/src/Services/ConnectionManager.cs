using System.Collections.Concurrent;
using System.Net.WebSockets;

namespace WebSocketApp.Services;

public class ConnectionManager
{
    private readonly ConcurrentDictionary<string, WebSocket> _clients = new();

    public void AddClient(string id, WebSocket socket)
    {
        // Si l'ID existe déjà (reconnexion rapide), on récupère l'ancien
        if (_clients.TryRemove(id, out var oldSocket))
        {
            // On le tue immédiatement pour libérer les ressources
            oldSocket.Abort(); 
            oldSocket.Dispose();
            Console.WriteLine($"[!] Ancien socket pour {id} supprimé (doublon)");
        }
        _clients[id] = socket;
    }

    public async Task RemoveClient(string id, WebSocket socket)
    {
        // On vérifie si le socket stocké est bien celui qui demande sa suppression
        if (_clients.TryGetValue(id, out var storedSocket) && ReferenceEquals(storedSocket, socket))
        {
            _clients.TryRemove(id, out _);
            
            if (socket.State == WebSocketState.Open || socket.State == WebSocketState.CloseReceived)
            {
                try {
                    using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2));
                    await socket.CloseAsync(WebSocketCloseStatus.NormalClosure, "Closing", cts.Token);
                } catch { /* Ignorer */ }
            }
        }
        // Dans tous les cas, on libère les ressources du socket qui ferme
        socket.Dispose();
    }

    public IEnumerable<string> GetAllIds() => _clients.Keys;

    public WebSocket? GetClient(string id) => _clients.TryGetValue(id, out var socket) ? socket : null;

    public IEnumerable<WebSocket> GetAllExcept(string exceptId)
    {
        return _clients
            .Where(kv => kv.Key != exceptId)
            .Select(kv => kv.Value);
    }
}