import React, { useState, useEffect } from 'react';
import './PanelControl.css';

import VideoCanvas from './plugins/videoCanvas/VideoCanvas';
import MotorControl from './plugins/motorControl/MotorControl';
import LightControl from './plugins/lightControl/LightControl';
import StatusLogs from './plugins/statusLogs/StatusLogs';
import SystemActions from './plugins/systemAction/SystemActions';
import Telemetry from './plugins/telemetry/Telemetry';
import wsService from '../../services/WebsocketService';
import { TrackController, Train } from '../../models/Protocol';
import Sidebar from './sidebar/Sidebar';
import TurnoutControl from './turnoutControl/TurnoutControl';

interface PanelControlProps {
  myId: string;
  trains: Train[];
  trackControllers: TrackController[];
  statusLogs: string[];          
  addLog: (msg: string) => void;
}

const PanelControl: React.FC<PanelControlProps> = ({
  myId,
  trains,
  trackControllers,
  statusLogs, 
  addLog
}) => {
  const [currentTrain, setCurrentTrain] = useState<Train | null>(null);

  const handleRefreshTrackController = () => {
    addLog("Mise à jour de la liste des track Controllers actifs...");
    wsService.sendGetTrackController();
  };

  const handleRefreshTrains = () => {
    addLog("Mise à jour de la liste des trains actifs...");
    wsService.sendGetTrains();
  };

  useEffect(() => {
    wsService.sendGetTrains();
  }, []);

  useEffect(() => {
    if (currentTrain) {
      const updatedTrain = trains.find(t => t.id === currentTrain.id);
      if (updatedTrain) {
        setCurrentTrain(updatedTrain);
      } else {
        addLog(`Alert: Target ${currentTrain.id} is no longer available.`);
        setCurrentTrain(null);
      }
    }
  }, [trains, currentTrain?.id]);

  const handleSelectTrainId = (id: string | null) => {
    if (!id) {
      setCurrentTrain(null);
      return;
    }
    const selected = trains.find(t => t.id === id) || null;
    setCurrentTrain(selected);
  };

  return (
    <div className="app-container">
      
      <Sidebar 
        trains={trains} 
        targetTrainId={currentTrain?.id || null} 
        setTargetTrainId={handleSelectTrainId} 
        refreshTrains={handleRefreshTrains} 
      />

      <main className="control-panel">
        <div className="panel-content">
          {currentTrain ? (
            <div className="actions-section" key={currentTrain.id}>
              <div className="main-view">
                <header className="panel-header">
                  <h2>Active Train: <span className="highlight">{currentTrain.id}</span></h2>
                </header>
                <VideoCanvas 
                  myId={myId} 
                  targetTrainId={currentTrain.id} 
                />
              </div>
              
              <div className="side-controls">
                <MotorControl targetTrain={currentTrain} addLog={addLog} />
                <LightControl targetTrain={currentTrain} addLog={addLog} />
                <SystemActions targetTrainId={currentTrain.id} />
                <Telemetry targetTrainId={currentTrain.id} />
              </div>
            </div>
          ) : (
            <div className="no-target-card">
              <h3>Aucun train sélectionné</h3>
              <p>Sélectionnez un convoi dans la barre latérale pour prendre le contrôle.</p>
            </div>
          )}
        </div>
        <StatusLogs logs={statusLogs} />
      </main>

      <TurnoutControl
        trackControllers={trackControllers}
        addLog={addLog}
      />
    </div>
  );
};

export default PanelControl;