import React, { useState } from 'react';
import wsService from '../../../../services/WebsocketService';
import './MotorControl.css';

/**
 * @interface MotorControlProps
 * @brief Properties required for the Motor Control plugin.
 */
interface MotorControlProps {
  targetId: string;
  addLog: (text: string) => void;
}

/** * @type Direction
 * fwd: Forward movement
 * rev: Reverse movement
 */
type Direction = 'fwd' | 'rev';

/**
 * @component MotorControl
 * @brief Dashboard plugin for controlling train traction, speed, and direction.
 */
const MotorControl: React.FC<MotorControlProps> = ({ targetId, addLog }) => {
  const [speed, setSpeed] = useState<number>(0);
  const [direction, setDirection] = useState<Direction>('fwd');

  /**
   * @brief Synchronizes local state and transmits motor commands via WebSocket.
   * @param newSpeed Integer from 0 to 100 representing percentage of max speed.
   * @param newDir Direction of travel ('fwd' or 'rev').
   */
  const update = (newSpeed: number, newDir: Direction) => {
    if (!targetId) return;

    setSpeed(newSpeed);
    setDirection(newDir);

    wsService.send(JSON.stringify({
      type: "motor", 
      target: targetId,
      payload: { 
        speed: String(newSpeed), 
        dir: newDir 
      },
      timestamp: Math.floor(Date.now() / 1000)
    }));

    addLog(`Motor: ${newSpeed}% | Direction: ${newDir.toUpperCase()}`);
  };

  return (
    <div className="card motor-card">
      <h3>Traction Control</h3>
      
      {/* Visual display of current throttle percentage */}
      <div className="speed-display">
        <span className="speed-value">{speed}</span>
        <span className="speed-unit">% vMax</span>
      </div>

      {/* Throttle slider (0-100) */}
      <input 
        type="range" 
        min="0" 
        max="100" 
        value={speed} 
        onChange={(e: React.ChangeEvent<HTMLInputElement>) => 
          update(parseInt(e.target.value, 10), direction)
        }
        className="speed-slider"
      />

      {/* Direction Selection Buttons */}
      <div className="direction-toggle">
        <button 
          className={direction === 'fwd' ? 'active' : ''} 
          onClick={() => update(speed, 'fwd')}
        >
          ▲ FORWARD
        </button>
        <button 
          className={direction === 'rev' ? 'active' : ''} 
          onClick={() => update(speed, 'rev')}
        >
          ▼ REVERSE
        </button>
      </div>

      {/* Emergency Braking Button */}
      <button onClick={() => update(0, direction)} className="emergency-stop">
        ⚠️ EMERGENCY STOP
      </button>
    </div>
  );
};

export default MotorControl;