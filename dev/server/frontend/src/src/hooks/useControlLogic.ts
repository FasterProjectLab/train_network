import { useState, useCallback, useEffect } from 'react';
import wsService from '../services/WebsocketService';
import { ProtocolConstants, MessageEnvelope } from '../models/Protocol';

export interface ControlLogicReturn {
  isConnected: boolean;
  trains: string[];
  statusLogs: string[];
  handleConnect: () => void;
  addLog: (text: string) => void;
}

/**
 * @brief Custom hook managing the application's core control logic and WebSocket events.
 * Handles train session tracking, system logging, and binary data (images) dispatching.
 */
export function useControlLogic(myId: string): ControlLogicReturn {
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [trainSessions, setTrainSessions] = useState<Record<string, string>>({});
  const [statusLogs, setStatusLogs] = useState<string[]>([]);

  /**
   * @brief Formats and adds a new entry to the system logs.
   */
  const addLog = useCallback((text: string) => {
    setStatusLogs((prev) => [`${new Date().toLocaleTimeString()} - ${text}`, ...prev].slice(0, 50));
  }, []);

  useEffect(() => {
    /**
     * @brief Main WebSocket message handler.
     * Dispatches binary data to the UI and parses JSON envelopes for state updates.
     */
    const onMessageReceived = (data: string | Blob) => {
      // Handle raw binary data (Camera frames from ESP32)
      if (data instanceof Blob) {
        window.dispatchEvent(new CustomEvent('ws-image', { detail: data }));
        return;
      }

      try {
        const envelope: MessageEnvelope = JSON.parse(data);
        switch (envelope.type) {
          case ProtocolConstants.ActionConnAck:
            addLog("Connected to dispatch server");
            setIsConnected(true);

            // Synchronize the initial list of active trains provided by the server
            if (envelope.payload?.active_trains) {
                const initialSessions: Record<string, string> = {};
                
                envelope.payload.active_trains.forEach((s: any) => {
                    // Map train IDs to their current active session IDs
                    initialSessions[s.trainId] = s.sessionId;
                });
                
                setTrainSessions(initialSessions);
                addLog(`${Object.keys(initialSessions).length} train(s) detected`);
            }
            break;

          case "train_status_change":
            const { trainId, sessionId, status } = envelope.payload;

            if (status === "connected") {
              setTrainSessions((prev) => {
                // Check if this is a reconnection or a completely new device
                if (prev[trainId] && prev[trainId] !== sessionId) {
                  addLog(`[SYSTEM] ${trainId} reconnected (New session)`);
                } else {
                  addLog(`[SYSTEM] ${trainId} is now online`);
                }
                
                return {
                  ...prev,
                  [trainId]: sessionId
                };
              });
            } 
            else if (status === "disconnected") {
              setTrainSessions((prev) => {
                // Security check: only remove if the session ID matches the one in memory.
                // This prevents accidentally deleting a new session if an old disconnect message arrives late.
                if (prev[trainId] === sessionId) {
                  const next = { ...prev };
                  delete next[trainId];
                  addLog(`[SYSTEM] ${trainId} went offline`);
                  return next;
                }
                return prev;
              });
            }
            break;
        }
      } catch (e) { 
        console.error("JSON Parsing Error:", e); 
      }
    };

    wsService.addListener(onMessageReceived);
    return () => wsService.removeListener(onMessageReceived);
  }, [addLog]);

  /**
   * @brief Initiates WebSocket connection and sends the initial authentication envelope.
   */
  const handleConnect = () => {
    wsService.connect(
      myId,
      ProtocolConstants.TypeUser,
      () => {
        const msg: MessageEnvelope = {
          type: ProtocolConstants.ActionConn,
          source: myId,
          target: ProtocolConstants.TargetServer,
          payload: { role: "admin" },
          timestamp: Math.floor(Date.now() / 1000)
        };
        wsService.send(JSON.stringify(msg));
      },
      () => {
        setIsConnected(false);
        addLog("Disconnected from server");
      }
    );
  };

  const trains = Object.keys(trainSessions);
  return { isConnected, trains, statusLogs, handleConnect, addLog };
}