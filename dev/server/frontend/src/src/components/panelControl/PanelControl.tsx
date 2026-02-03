import React, { useState, useEffect } from 'react';
import './PanelControl.css';

import Sidebar from './plugins/sidebar/Sidebar';
import VideoCanvas from './plugins/videoCanvas/VideoCanvas';
import MotorControl from './plugins/motorControl/MotorControl';
import LightControl from './plugins/lightControl/LightControl';
import StatusLogs from './plugins/statusLogs/StatusLogs';
import SystemActions from './plugins/systemAction/SystemActions';
import wsService from '../../services/WebsocketService';
import { ProtocolConstants } from '../../models/Protocol';
import Telemetry from './plugins/telemetry/Telemetry';

/**
 * @interface PanelControlProps
 * @brief Properties passed from the App root to manage global state and logging.
 */
interface PanelControlProps {
  myId: string;
  trains: string[];
  targetId: string;
  setTargetId: (id: string) => void;
  statusLogs: string[];          
  addLog: (msg: string) => void;
}

/**
 * @component PanelControl
 * @brief Main Dashboard View
 * * Orchestrates the video stream, motor/light plugins, and system telemetry.
 */
const PanelControl: React.FC<PanelControlProps> = ({
  myId,
  trains,
  targetId,
  setTargetId,
  statusLogs, 
  addLog
}) => {
  const [isStreaming, setIsStreaming] = useState<boolean>(false);

  /**
   * @note Target Synchronization
   * Resets the stream and selection if the targeted train disconnects from the server.
   */
  useEffect(() => {
    if (targetId && !trains.includes(targetId)) {
      addLog(`Alert: Target ${targetId} is no longer available.`);
      stopStream();
      setTargetId(""); // Reset local selection
    }
  }, [trains, targetId, setTargetId, addLog]);

  /**
   * @brief Subscribes to the camera feed of the current target
   */
  const startStream = () => {
    if (!targetId) return;
    
    wsService.send(JSON.stringify({
      type: ProtocolConstants.ActionSubscibe,
      target: targetId,
      payload: "camera"
    }));
    
    setIsStreaming(true);
    addLog(`Starting camera stream on ${targetId}`);
  };

  /**
   * @brief Unsubscribes from the camera feed
   */
  const stopStream = () => {
    if (!targetId) return;
    
    wsService.send(JSON.stringify({
      type: ProtocolConstants.ActionUnsubscribe,
      target: targetId,
      payload: "camera"
    }));
    
    setIsStreaming(false);
    addLog(`Stopping camera stream on ${targetId}`);
  };

  return (
    <div className="app-container">
      {/* Navigation & Device List */}
      <Sidebar 
        trains={trains} 
        targetId={targetId} 
        setTargetId={setTargetId} 
        refreshTrains={() => wsService.requestActiveTrains()} 
      />

      <main className="control-panel">
        {targetId ? (
          <div className="panel-content">
            <header className="panel-header">
              <h2>Active Target: <span className="highlight">{targetId}</span></h2>
            </header>
            
            <div className="actions-section">
              {/* Left Column: Video Feed & Real-time Logs */}
              <div className="main-view">
                <VideoCanvas 
                  myId={myId} 
                  targetId={targetId} 
                  isStreaming={isStreaming} 
                  onStart={startStream} 
                  onStop={stopStream} 
                />
                <StatusLogs logs={statusLogs} />
              </div>
              
              {/* Right Column: Hardware Commands & Telemetry */}
              <div className="side-controls">
                <MotorControl targetId={targetId} addLog={addLog} />
                <LightControl targetId={targetId} addLog={addLog} />
                <SystemActions targetId={targetId} />
                <Telemetry targetId={targetId} />
              </div>
            </div>
          </div>
        ) : (
          /* Empty State: Prompt user to select a device */
          <div className="no-target">
            <div className="card">
              <h3>Ready for Action</h3>
              <p>Please select a train from the sidebar to take control.</p>
            </div>
          </div>
        )}
      </main>
    </div>
  );
};

export default PanelControl;