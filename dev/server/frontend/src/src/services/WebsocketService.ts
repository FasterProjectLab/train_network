import { MessageEnvelope, ProtocolConstants } from '../models/Protocol';

type WSListener = (data: string | Blob) => void;

/**
 * @class WebsocketService
 * @brief Singleton service to manage WebSocket lifecycle, heartbeats, and message distribution.
 */
class WebsocketService {
  private myId: string = "";
  private socket: WebSocket | null = null;
  private listeners: Set<WSListener> = new Set();
  private pingInterval: number | null = null;

  /**
   * @brief Establishes a connection to the WebSocket server
   */
  connect(
    id: string, 
    type: string, 
    onOpen: () => void, 
    onClose: () => void
  ): void {
    this.myId = id;
    if (this.socket) this.disconnect();

    this.socket = new WebSocket(`ws://${window.location.hostname}:8080/ws/${id}?type=${type}`);

    this.socket.onopen = () => {
      this.startPing(id); 
      onOpen();
    };

    this.socket.onclose = () => {
      this.stopPing(); 
      onClose();
    };

    this.socket.onerror = (err) => console.error("WebSocket Error:", err);

    this.socket.onmessage = (event: MessageEvent) => {
      this.listeners.forEach(listener => listener(event.data));
    };
  }

  private startPing(sourceId: string): void {
    this.stopPing();
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

  private stopPing(): void {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
  }

  addListener(listener: WSListener) { this.listeners.add(listener); }
  removeListener(listener: WSListener) { this.listeners.delete(listener); }

  send(data: string): void {
    if (this.socket?.readyState === WebSocket.OPEN) {
      this.socket.send(data);
    }
  }

  disconnect(): void {
    if (this.socket) {
      this.socket.close();
      this.socket = null;
    }
  }

  sendAckWelcom() {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionConnAck,
      source: this.myId,
      target: ProtocolConstants.TargetServer,
      payload: { role: "admin" },
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendGetTrackController() {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionGetTrackControllers,
      source: this.myId,
      target: ProtocolConstants.TargetServer,
      payload: null,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendGetTrains(): void {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionGetTrains,
      source: this.myId,
      target: ProtocolConstants.TargetServer,
      payload: null,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendGetTurnoutForTrack(trackId: string) {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionGetTurnouts,
      source: this.myId,
      target: trackId,
      payload: null,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendToggleTurnout(trackId: string, turnoutId: string, nextPosition: string) {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionSetTurnouts,
      source: this.myId,
      target: trackId,
      payload: { 
        id: turnoutId, 
        position: nextPosition 
      },
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendStartCamera(trainId: string) {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionSubscibe,
      source: this.myId,
      tag: "camera",
      target: trainId,
      payload: null,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendStopCamera(trainId: string) {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionUnsubscribe,
      source: this.myId,
      tag: "camera",
      target: trainId,
      payload: null,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendGetTrainStatus(trainId: string) {
    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionGetTrainStatus,
      source: this.myId,
      target: trainId,
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendLightState = (trainId: string, state: boolean) => {
    if (!trainId ) return;

    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionLight,
      source: this.myId,
      target: trainId,
      payload: { status: state ? "ON" : "OFF" },
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));
  }

  sendMotorCommand = (trainId: string, newSpeed: string, newDir: string) => {
    if (!trainId ) return;

    const msg: MessageEnvelope = {
      type: ProtocolConstants.ActionMotor,
      source: this.myId,
      target: trainId,
      payload: { 
            speed: newSpeed, 
            dir: newDir 
      },
      timestamp: Math.floor(Date.now() / 1000)
    };
    wsService.send(JSON.stringify(msg));

  }
}


const wsService = new WebsocketService();
export default wsService;