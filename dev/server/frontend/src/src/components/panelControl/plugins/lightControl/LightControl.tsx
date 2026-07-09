import React, { useState } from 'react';
import wsService from '../../../../services/WebsocketService';
import './LightControl.css';
import { Train } from '../../../../models/Protocol';


interface LightControlProps {
  targetTrain: Train;
  addLog: (text: string) => void;
}

const LightControl: React.FC<LightControlProps> = ({ targetTrain, addLog }) => {
  const [isFlashing, setIsFlashing] = useState<boolean>(false);

  const isOn = targetTrain.status?.light ?? false;


  const sendLightState = (state: boolean) => {
    wsService.sendLightState(targetTrain.id, state);
    wsService.sendGetTrainStatus(targetTrain.id);
    addLog(`Lighting: ${state ? 'ENABLED' : 'DISABLED'}`);
  };

  const handleFlashStart = () => {
    setIsFlashing(true);
    sendLightState(true);
  };

  const handleFlashEnd = () => {
    if (isFlashing) {
      setIsFlashing(false);
      sendLightState(false);
    }
  };

  const toggleLight = () => {
    sendLightState(!isOn);
  };

  return (
    <div className="card light-card">
      <h3>Lighting Control</h3>

      <div className={`light-status-indicator ${isOn ? 'active' : ''}`}>
        <div className="bulb"></div>
        <span>{isOn ? 'ACTIVE' : 'INACTIVE'}</span>
      </div>

      <div className="light-controls-grid">
        <button 
          className={`control-btn toggle-btn ${isOn ? 'on' : ''}`}
          onClick={toggleLight}
        >
          {isOn ? 'Turn Off' : 'Turn On'}
        </button>

        <button 
          className="control-btn momentary-btn"
          onMouseDown={handleFlashStart}
          onMouseUp={handleFlashEnd}
          onMouseLeave={handleFlashEnd} 
          onTouchStart={handleFlashStart}
          onTouchEnd={handleFlashEnd}
        >
          Flash (Hold)
        </button>
      </div>
    </div>
  );
};

export default LightControl;