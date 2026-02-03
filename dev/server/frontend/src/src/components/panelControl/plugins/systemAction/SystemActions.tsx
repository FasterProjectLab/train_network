import React, { useState } from 'react';
import wsService from '../../../../services/WebsocketService';
import { ProtocolConstants } from '../../../../models/Protocol';
import './SystemActions.css';

interface SystemActionsProps {
  targetId: string;
}

const SystemActions: React.FC<SystemActionsProps> = ({ targetId }) => {
  const DEFAULT_OTA_URL = "https://192.168.10.1/firmware.bin";
  const [otaUrl, setOtaUrl] = useState<string>(DEFAULT_OTA_URL);

  const sendSystemCommand = (action: string, data: any) => {
    if (!targetId) return;

    wsService.send(JSON.stringify({
      type: ProtocolConstants.ActionSystem, 
      target: targetId,
      payload: { action: action, ...data},
      timestamp: Math.floor(Date.now() / 1000)
    }));
  };

  const triggerOTA = () => {
    const message = `⚠️ WARNING: Start firmware update?\nSource: ${otaUrl}\n\nThe device will be unavailable for several minutes.`;
    if (window.confirm(message)) {
      sendSystemCommand("update_firmware", { src: otaUrl });
    }
  };

  const triggerReboot = () => {
    if (window.confirm("Are you sure you want to reboot the device?")) {
      sendSystemCommand("reboot", null);
    }
  };

  return (
    <div className="card maintenance-card">
      <h3>System Maintenance</h3>
      
      <div className="ota-config-container">
        <label className="ota-label">Firmware URL</label>
        <input 
          type="text" 
          className="ota-input-field"
          value={otaUrl} 
          onChange={(e) => setOtaUrl(e.target.value)}
          placeholder="Enter firmware URL..."
        />
      </div>

      <div className="actions-grid">
        {/* Firmware Update Button */}
        <button onClick={triggerOTA} className="sys-btn ota-btn">
          <span className="btn-icon">📥</span>
          OTA Update
        </button>

        {/* Device Reboot Button */}
        <button onClick={triggerReboot} className="sys-btn reboot-btn">
          <span className="btn-icon">🔄</span>
          Reboot
        </button>
      </div>
    </div>
  );
};

export default SystemActions;