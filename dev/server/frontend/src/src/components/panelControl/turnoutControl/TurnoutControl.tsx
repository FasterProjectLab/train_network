import React, { useEffect } from 'react';
import { TrackController, TurnoutDevice } from '../../../models/Protocol';
import wsService from '../../../services/WebsocketService';
import './TurnoutControl.css';

interface TurnoutControlProps {
  trackControllers: TrackController[];
  addLog: (text: string) => void;
}

const TurnoutControl: React.FC<TurnoutControlProps> = ({
  trackControllers,
  addLog
}) => {

  useEffect(() => {   
    wsService.sendGetTrackController();
  }, []);

  const handleRefreshTrack = (trackId: string) => {
    addLog(`[SYSTEM] Rafraîchissement manuel demandé pour ${trackId}`);
    wsService.sendGetTurnoutForTrack(trackId);
  };

  const handleToggleTurnout = (trackId: string, turnoutId: string, currentPosition: 'normal' | 'divergent') => {
    const nextPosition = currentPosition === 'normal' ? 'divergent' : 'normal';
    addLog(`[ACTION] Boîtier ${trackId} | Changement d'aiguillage ${turnoutId} ➔ ${nextPosition.toUpperCase()}`);
    
    wsService.sendToggleTurnout(trackId, turnoutId, nextPosition);
  };

  return (
    <div className="turnout-control-panel main_ctrl">
      <h2>🎛️ Contrôle des Aiguillages</h2>

      {trackControllers.length === 0 ? (
        /* Utilisation de la notion .no-target-card du premier CSS */
        <div className="no-target-card">
          <h3>Aucun contrôleur</h3>
          <p>Aucun Track Controller détecté sur le réseau.</p>
        </div>
      ) : (
        trackControllers.map((track: TrackController) => {
          return (
            <div key={track.id} className="track-card">
              
              {/* En-tête du Boîtier */}
              <div className="track-header">
                <div>
                  <h3>Boîtier : <span className="highlight">{track.id}</span></h3>
                  <small className="status-text">Statut: {track.status}</small>
                </div>
                <button 
                  className="btn btn-refresh" 
                  onClick={() => handleRefreshTrack(track.id)}
                >
                  🔄 Rafraîchir
                </button>
              </div>

              {/* Liste des Aiguillages du Boîtier */}
              <div className="turnouts-grid">
                {!track.turnouts || track.turnouts.length === 0 ? (
                  <p className="no-data-text">
                    Aucun aiguillage synchronisé. Cliquez sur Rafraîchir.
                  </p>
                ) : (
                  track.turnouts.map((turnout: TurnoutDevice) => {
                    const isNormal = turnout.position === 'normal';
                    
                    return (
                      <div 
                        key={turnout.id} 
                        className={`turnout-item ${isNormal ? 'status-normal' : 'status-divergent'}`}
                      >
                        <div className="turnout-id">Id: {turnout.id}</div>
                        
                        <div className="turnout-status">
                          État : <span className="status-badge">
                            {turnout.position.toUpperCase()}
                          </span>
                        </div>

                        <button
                          className="btn btn-toggle"
                          onClick={() => handleToggleTurnout(track.id, turnout.id, turnout.position)}
                        >
                          Basculer
                        </button>
                      </div>
                    );
                  })
                )}
              </div>

            </div>
          );
        })
      )}
    </div>
  );
};

export default TurnoutControl;