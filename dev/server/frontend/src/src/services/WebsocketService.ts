type WSListener = (data: string | Blob) => void;

/**
 * @class WebsocketService
 * @brief Singleton service to manage WebSocket lifecycle, heartbeats, and message distribution.
 */
class WebsocketService {
  private socket: WebSocket | null = null;
  private listeners: Set<WSListener> = new Set();
  private pingInterval: number | null = null;

  /**
   * @brief Establishes a connection to the WebSocket server
   * @param id Unique identifier for this client
   * @param type Client type (e.g., 'controller')
   * @param onOpen Callback executed when connection is established
   * @param onClose Callback executed when connection is lost
   */
  connect(
    id: string, 
    type: string, 
    onOpen: () => void, 
    onClose: () => void
  ): void {
    // Clean up existing connection if necessary
    if (this.socket) this.disconnect();

    this.socket = new WebSocket(`ws://${window.location.hostname}:8080/ws/${id}?type=${type}`);

    this.socket.onopen = () => {
      // Start the heartbeat (ping) mechanism immediately upon connection
      this.startPing(id); 
      onOpen();
    }

    this.socket.onclose = () => {
      // Stop the heartbeat when the connection is closed
      this.stopPing(); 
      onClose();
    };

    this.socket.onerror = (err) => console.error("WebSocket Error:", err);

    this.socket.onmessage = (event: MessageEvent) => {
      // Broadcast incoming data to all registered listeners
      this.listeners.forEach(listener => listener(event.data));
    };
  }

  /**
   * @brief Starts a periodic ping to keep the connection alive
   * @param sourceId The ID of the sender to include in the ping envelope
   */
  private startPing(sourceId: string): void {
    this.stopPing(); // Safety: clear any existing interval before starting a new one
    
    // Sends a ping every 5 seconds (matching the ESP32 heartbeat interval)
    this.pingInterval = window.setInterval(() => {
      const pingPayload = JSON.stringify({
        type: "ping",
        source: sourceId,
        target: "server",
        timestamp: Date.now(),
        payload: {}
      });
      this.send(pingPayload);
    }, 5000);
  }

  /**
   * @brief Clears the heartbeat interval timer
   */
  private stopPing(): void {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
  }

  /**
   * @brief Requests the current list of active trains from the server
   */
  requestActiveTrains(): void {
    const request = JSON.stringify({
      type: "get_trains",
      target: "server",
      timestamp: Math.floor(Date.now() / 1000),
      payload: {}
    });
    this.send(request);
  }

  // Listener management for components to subscribe to specific messages
  addListener(listener: WSListener) { this.listeners.add(listener); }
  removeListener(listener: WSListener) { this.listeners.delete(listener); }

  /**
   * @brief Transmits raw string data over the socket if open
   */
  send(data: string): void {
    if (this.socket?.readyState === WebSocket.OPEN) {
      this.socket.send(data);
    }
  }

  /**
   * @brief Closes the connection and resets the socket instance
   */
  disconnect(): void {
    if (this.socket) {
      this.socket.close();
      this.socket = null;
    }
  }
}

// Export a single instance (Singleton pattern)
const wsService = new WebsocketService();
export default wsService;