import React, { useState } from 'react';
import wsService from '../../../../services/WebsocketService';
import './LightControl.css';

/**
 * @interface LightControlProps
 * @brief Defines properties for the Light Control plugin.
 */
interface LightControlProps {
  targetId: string;
  addLog: (text: string) => void;
}

/**
 * @component LightControl
 * @brief Plugin for managing train lighting, including toggle and momentary (flash) modes.
 */
const LightControl: React.FC<LightControlProps> = ({ targetId, addLog }) => {
  const [isOn, setIsOn] = useState<boolean>(false);
  const [isFlashing, setIsFlashing] = useState<boolean>(false);

  /**
   * @brief Transmits the light state to the target ESP32 via WebSocket.
   * @param state True for ON, False for OFF.
   * @param isFlashAction Flag to distinguish between a toggle and a momentary flash.
   */
  const sendLightState = (state: boolean, isFlashAction: boolean = false) => {
    if (!targetId) return;

    setIsOn(state);
    
    wsService.send(JSON.stringify({
      type: "light",
      target: targetId,
      payload: { status: state ? "ON" : "OFF" },
      timestamp: Math.floor(Date.now() / 1000)
    }));

    addLog(`Lighting: ${state ? 'ENABLED' : 'DISABLED'}`);
  };

  /**
   * @brief Handles the start of a momentary flash (mouse/touch down).
   */
  const handleFlashStart = () => {
    setIsFlashing(true);
    sendLightState(true, true);
  };

  /**
   * @brief Handles the end of a momentary flash (mouse/touch up or leave).
   */
  const handleFlashEnd = () => {
    if (isFlashing) {
      setIsFlashing(false);
      sendLightState(false, true);
    }
  };

  /**
   * @brief Toggles the current light state.
   */
  const toggleLight = () => {
    sendLightState(!isOn);
  };

  return (
    <div className="card light-card">
      <h3>Lighting Control</h3>

      {/* Visual status indicator of the current light state */}
      <div className={`light-status-indicator ${isOn ? 'active' : ''}`}>
        <div className="bulb"></div>
        <span>{isOn ? 'ACTIVE' : 'INACTIVE'}</span>
      </div>

      <div className="light-controls-grid">
        {/* Toggle Button */}
        <button 
          className={`control-btn toggle-btn ${isOn ? 'on' : ''}`}
          onClick={toggleLight}
        >
          {isOn ? 'Turn Off' : 'Turn On'}
        </button>

        {/* Momentary Flash Button (Active only while pressed) */}
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