using System.Collections.Concurrent;
using System.Net.WebSockets;
using WebSocketApp.Models;

namespace WebSocketApp.Services;

/// <summary>
/// Manages active WebSocket sessions, Pub/Sub subscriptions, and hardware mappings (SSRC).
/// Utilizes thread-safe collections to handle high-concurrency train control traffic.
/// </summary>
public class ConnectionManager
{
    // Maps a Client ID (e.g., Train MAC) to its active sessions
    private readonly ConcurrentDictionary<string, List<ClientSession>> _clients = new();
    
    // Reverse lookup: Maps a specific ConnectionId to a Client ID
    private readonly ConcurrentDictionary<string, string> _sessionToId = new();

    // Pub/Sub: Maps "trainId:tag" to a set of User IDs subscribed to it
    private readonly ConcurrentDictionary<string, HashSet<string>> _topicSubscriptions = new();

    // User tracking: Maps User ID to the topics they are currently watching
    private readonly ConcurrentDictionary<string, HashSet<string>> _userInterests = new();

    // Media Routing: Maps SessionId to an SSRC (Synchronization Source ID for RTP/Video)
    private readonly ConcurrentDictionary<string, uint> _sessionSsrcs = new();

    #region SSRC Mapping (Media Routing)

    public void RegisterSessionSsrc(string sessionId, uint ssrc)
    {
        _sessionSsrcs[sessionId] = ssrc;
        Console.WriteLine($"[MAPPING] Session: {sessionId} -> SSRC: 0x{ssrc:X8}");
    }

    public uint GetSsrcBySessionId(string sessionId) =>
        _sessionSsrcs.TryGetValue(sessionId, out var ssrc) ? ssrc : 0;

    public string? GetSessionIdBySsrc(uint ssrc) =>
        _sessionSsrcs.FirstOrDefault(x => x.Value == ssrc).Key;

    public string? GetTrainIdBySsrc(uint ssrc)
    {
        var sessionId = GetSessionIdBySsrc(ssrc);
        return sessionId != null && _sessionToId.TryGetValue(sessionId, out var trainId) ? trainId : null;
    }

    #endregion

    #region Session Management

    /// <summary>
    /// Registers a new WebSocket connection into the manager.
    /// </summary>
    public void AddClient(string id, string connectionId, WebSocket socket, string type)
    {
        var session = new ClientSession(id, connectionId, socket, type);
        _sessionToId.TryAdd(connectionId, id);

        _clients.AddOrUpdate(id, 
            new List<ClientSession> { session }, 
            (key, list) => {
                lock(list) { list.Add(session); }
                return list;
            });
    }

    /// <summary>
    /// Removes a session, cleans up subscriptions, and safely closes the WebSocket.
    /// </summary>
    public async Task RemoveClient(string sessionId)
    {
        if (!_sessionToId.TryRemove(sessionId, out var userId)) return;

        // Revoke media mapping if it exists
        _sessionSsrcs.TryRemove(sessionId, out _);

        if (_clients.TryGetValue(userId, out var list))
        {
            ClientSession? sessionToRemove;
            bool isLastSession;

            lock (list)
            {
                sessionToRemove = list.FirstOrDefault(s => s.ConnectionId == sessionId);
                if (sessionToRemove != null) list.Remove(sessionToRemove);
                isLastSession = (list.Count == 0);
            }

            // If the user has no more active sessions, clean up their subscriptions
            if (isLastSession)
            {
                _clients.TryRemove(userId, out _);
                
                if (_userInterests.TryRemove(userId, out var interests))
                {
                    foreach (var topic in interests)
                    {
                        if (_topicSubscriptions.TryGetValue(topic, out var subscribers))
                        {
                            lock(subscribers) { subscribers.Remove(userId); }
                        }
                    }
                }
            }

            if (sessionToRemove != null) await CloseSocketSafe(sessionToRemove.Socket);
        }
    }

    private async Task CloseSocketSafe(WebSocket socket)
    {
        if (socket.State == WebSocketState.Open || socket.State == WebSocketState.CloseReceived)
        {
            try 
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(2));
                await socket.CloseAsync(WebSocketCloseStatus.NormalClosure, "Server Closing", cts.Token);
            }
            catch { /* Socket might already be aborted */ }
        }
        socket.Dispose(); 
    }

    #endregion

    #region Pub/Sub Logic

    public void Subscribe(string userId, string trainId, string tag)
    {
        string topic = $"{trainId}:{tag.ToLower()}";

        var subscribers = _topicSubscriptions.GetOrAdd(topic, _ => new HashSet<string>());
        lock(subscribers) { subscribers.Add(userId); }

        var interests = _userInterests.GetOrAdd(userId, _ => new HashSet<string>());
        lock(interests) { interests.Add(topic); }
    }

    public void Unsubscribe(string userId, string trainId, string tag)
    {
        string topic = $"{trainId}:{tag.ToLower()}";
        
        if (_topicSubscriptions.TryGetValue(topic, out var subscribers))
        {
            lock(subscribers) { subscribers.Remove(userId); }
        }

        if (_userInterests.TryGetValue(userId, out var interests))
        {
            lock(interests) { interests.Remove(topic); }
        }
    }

    public IEnumerable<string> GetSubscribers(string trainId, string tag)
    {
        string topic = $"{trainId}:{tag.ToLower()}";
        if (_topicSubscriptions.TryGetValue(topic, out var subscribers))
        {
            lock(subscribers) { return subscribers.ToList(); }
        }
        return Enumerable.Empty<string>();
    }

    #endregion

    #region Queries

    public List<ClientSession> GetSessions(string id) 
    {
        if (_clients.TryGetValue(id, out var list))
        {
            lock(list) { return list.ToList(); }
        }
        return new List<ClientSession>();
    }

    /// <summary>
    /// Returns all User IDs (controllers) currently connected.
    /// </summary>
    public IEnumerable<string> GetAllUserIds()
    {
        return _clients
            .Where(kvp => kvp.Value.Any(session => session.Type == ProtocolConstants.TypeUser))
            .Select(kvp => kvp.Key);
    }

    /// <summary>
    /// Formats active train list for the UI (PanelControl).
    /// </summary>
    public object GetActiveTrainSessions()
    {
        return _clients
            .Where(kvp => kvp.Value.Any(s => s.Type == ProtocolConstants.TypeTrain))
            .Select(kvp => new {
                trainId = kvp.Key,
                sessionId = kvp.Value.LastOrDefault(s => s.Type == ProtocolConstants.TypeTrain)?.ConnectionId,
                status = "connected"
            })
            .ToList();
    }

public IEnumerable<string> GetUserTopics(string userId)
{
    if (_userInterests.TryGetValue(userId, out var topics))
    {
        lock (topics) 
        { 
            return topics.ToList(); 
        }
    }
    return Enumerable.Empty<string>();
}

    /// <summary>
    /// Forces closure of all sessions for a specific train. Used to resolve conflicts
    /// when a train reconnects before the old session timed out.
    /// </summary>
    public async Task ClearOldSessionsForTrain(string trainId)
    {
        if (_clients.TryGetValue(trainId, out var list))
        {
            List<ClientSession> sessionsToKill;
            lock (list) { sessionsToKill = list.ToList(); }

            foreach (var session in sessionsToKill)
            {
                Console.WriteLine($"[CLEANUP] Closing ghost session: {session.ConnectionId}");
                await RemoveClient(session.ConnectionId);
            }
        }
    }

    #endregion
}