import { useState, useCallback, useEffect } from 'react';
import wsService from '../services/WebsocketService';
import { ProtocolConstants, MessageEnvelope, TrackController, Train } from '../models/Protocol';

export interface ControlLogicReturn {
  isConnected: boolean;
  trains: Train[];
  trackControllers: TrackController[]; // Toujours un tableau pour l'affichage
  statusLogs: string[];
  handleConnect: () => void;
  addLog: (text: string) => void;
}

export function useControlLogic(myId: string): ControlLogicReturn {
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [trains, setTrains] = useState<Record<string, Train>>({});
  const [trackControllers, setTrackControllers] = useState<Record<string, TrackController>>({});
  const [statusLogs, setStatusLogs] = useState<string[]>([]);

  const addLog = useCallback((text: string) => {
    setStatusLogs((prev) => {
      const updatedLogs = [...prev, `${new Date().toLocaleTimeString()} - ${text}`];
      
      return updatedLogs.slice(-50);
    });
  }, []);

  useEffect(() => {
    const onMessageReceived = (data: string | Blob) => {
      if (data instanceof Blob) {
        window.dispatchEvent(new CustomEvent('ws-image', { detail: data }));
        return;
      }

      try {
        const envelope: MessageEnvelope = JSON.parse(data);
        switch (envelope.type) {
          case ProtocolConstants.ActionWelcom:
            if (envelope.payload) {
              if (envelope.payload.type == ProtocolConstants.TypeTrain) {
                wsService.sendGetTrains();
              } else if (envelope.payload.type == ProtocolConstants.TypeTrackController) {
                wsService.sendGetTrackController();
              }
            }
            break;
          case ProtocolConstants.ActionGoodbye:
            if (envelope.payload) {
              if (envelope.payload.type == ProtocolConstants.TypeTrain) {
                wsService.sendGetTrains();
              } else if (envelope.payload.type == ProtocolConstants.TypeTrackController) {
                wsService.sendGetTrackController();
              }
            }
            break;
          case ProtocolConstants.ActionGetTrackControllers:
            handleGetTrackController(envelope);
            break;
          case ProtocolConstants.ActionGetTrains:
            handleGetTrains(envelope);
            break;
          case ProtocolConstants.ActionGetTurnouts:
            handleGetTurnouts(envelope);
            break;
          case ProtocolConstants.ActionTrainStatus:
            handleTrainStatus(envelope);
            break;
        }
      } catch (e) { 
        console.error("JSON Parsing Error:", e); 
      }
    };

    wsService.addListener(onMessageReceived);
    return () => wsService.removeListener(onMessageReceived);
  }, [myId, addLog]);

  const handleConnect = () => {
    wsService.connect(
      myId,
      ProtocolConstants.TypeUser,
      () => { 
        setIsConnected(true);
        addLog("Connected to server");
      },
      () => {
        setIsConnected(false);
        addLog("Disconnected from server");
      }
    );
  };
  function handleTrainStatus(envelope: MessageEnvelope): void {
    const trainId = envelope.source;
    if (!trainId || !envelope.payload) return;

    setTrains((prev) => {
      const existingTrain = prev[trainId];
      
      const updatedStatus = {
        light: !!envelope.payload.light,
        motor: {
          speed: envelope.payload.motor?.speed ?? 0,
          dir: (envelope.payload.motor?.dir === 'rev' ? 'rev' : 'fwd') as 'fwd' | 'rev'
        },
        cameraActive: !!envelope.payload.camera_active,
        telemetryActive: !!envelope.payload.telemetry_active
      };

      return {
        ...prev,
        [trainId]: {
          id: trainId,
          sessionId: existingTrain?.sessionId ?? "",
          status: updatedStatus,
          lastUpdated: envelope.timestamp ?? Date.now()
        } as Train
      };
    });

    addLog(`[SYSTEM] Statut mis à jour pour le train ${trainId}`);
  }

  const handleGetTrackController = (envelope: MessageEnvelope) => {
    if (envelope.payload?.active) {
      const updatedControllers: Record<string, TrackController> = {};
      
      envelope.payload.active.forEach((tc: any) => {
        updatedControllers[tc.id] = {
          id: tc.id,
          sessionId: tc.sessionId,
          status: tc.status || "connected",
          turnouts: trackControllers[tc.id]?.turnouts || [] 
        };

        wsService.sendGetTurnoutForTrack(tc.id);
      });

      setTrackControllers(updatedControllers);
    }
  };

  const handleGetTrains = (envelope: MessageEnvelope) => {
    if (envelope.payload?.active) {
      const updatedTrains: Record<string, Train> = {};
      
      envelope.payload.active.forEach((s: Train) => {
        updatedTrains[s.id] = {
          id: s.id,
          sessionId: s.sessionId,
          status: trains[s.id]?.status,
          lastUpdated: trains[s.id]?.lastUpdated
        } as Train; 
      });
      
      setTrains(updatedTrains);
    }
  };

  const handleGetTurnouts = (envelope: MessageEnvelope) => {
    const trackId = envelope.source; 
    
    if (trackId && envelope.payload?.turnouts) {
      setTrackControllers((prev) => {
        if (!prev[trackId]) return prev;

        return {
          ...prev,
          [trackId]: {
            ...prev[trackId],
            turnouts: envelope.payload.turnouts 
          }
        };
      });
      addLog(`[SYSTEM] Synchronisation des aiguillages du boîtier ${trackId}`);
    }
  };

  const trainArray = Object.values(trains);
  
  const trackControllersArray = Object.values(trackControllers);

  return { 
    isConnected, 
    trains: trainArray, 
    trackControllers: trackControllersArray, 
    statusLogs, 
    handleConnect, 
    addLog 
  };
}