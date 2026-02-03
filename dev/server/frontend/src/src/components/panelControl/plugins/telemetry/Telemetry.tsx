import React, { useState, useEffect, useCallback } from 'react';
import wsService from '../../../../services/WebsocketService';
import { ProtocolConstants } from '../../../../models/Protocol';
import './Telemetry.css';

/**
 * @interface TelemetryViewProps
 * @brief Properties for the Telemetry plugin.
 */
interface TelemetryViewProps {
  targetId: string;
}

/**
 * @interface TelemetryData
 * @brief Structure of the telemetry payload received from the ESP32 firmware.
 */
interface TelemetryData {
  cpu0: number;       // CPU Core 0 utilization (%)
  cpu1: number;       // CPU Core 1 utilization (%)
  temp: number;       // Internal SoC temperature (°C)
  ram_free: number;   // Available Internal Heap (bytes)
  ram_min: number;    // Minimum ever recorded free Heap (bytes)
  psram_free: number; // Available External PSRAM (bytes)
  rssi: number;       // WiFi Signal Strength (dBm)
  uptime: number;     // Time since last boot (seconds)
  stack_mon: number;  // High-water mark for the main task stack (bytes)
}

/**
 * @component Telemetry
 * @brief Real-time system monitor for the targeted ESP32 device.
 * Handles subscription/unsubscription to the 'telemetry' data stream.
 */
const Telemetry: React.FC<TelemetryViewProps> = ({ targetId }) => {
  const [data, setData] = useState<TelemetryData | null>(null);
  const [isActive, setIsActive] = useState<boolean>(false);

  /**
   * @brief Toggles the telemetry data stream subscription.
   * @param active True to subscribe, False to unsubscribe.
   */
  const toggleTelemetry = useCallback((active: boolean) => {
    if (!targetId) return;

    wsService.send(JSON.stringify({
      type: active ? ProtocolConstants.ActionSubscibe : ProtocolConstants.ActionUnsubscribe,
      target: targetId,
      payload: "telemetry"
    }));
    
    setIsActive(active);
  }, [targetId]);

  /**
   * @brief WebSocket Listener Effect
   * Filters incoming messages for telemetry data belonging to the active target.
   */
  useEffect(() => {
    const handleTelemetry = (rawData: string | Blob) => {
      if (!isActive || typeof rawData !== 'string') return;
      try {
        const envelope = JSON.parse(rawData);
        // Only update if the message is 'telemetry' and from the selected target
        if (envelope.type === "telemetry" && envelope.source === targetId) {
          setData(envelope.payload);
        }
      } catch (e) {
        // Silent catch for non-JSON or malformed data
      }
    };

    wsService.addListener(handleTelemetry);

    return () => {
      wsService.removeListener(handleTelemetry);
      // Safety: Ensure we unsubscribe from the stream when the component unmounts
      if (isActive) {
        wsService.send(JSON.stringify({
          type: ProtocolConstants.ActionUnsubscribe,
          target: targetId,
          payload: "telemetry"
        }));
      }
    };
  }, [targetId, isActive]);

  // Helper to convert bytes to Megabytes with 2 decimal places
  const toMB = (bytes: number) => (bytes / (1024 * 1024)).toFixed(2);

  return (
    <div className="card telemetry-card">
      <div className="card-header">
        <h3>System Metrics</h3>
        <span className={`status-indicator ${isActive ? 'online' : 'offline'}`}>
          {isActive ? "Live Feed" : "Feed Inactive"}
        </span>
      </div>

      {isActive && data ? (
        <div className="telemetry-grid">
          {/* Processing Group */}
          <div className="telemetry-group">
            <div className="stat-row">
              <span>CPU Load</span>
              <strong>{data.cpu0}% | {data.cpu1}%</strong>
            </div>
            <div className="stat-row">
              <span>SoC Temp</span>
              <strong className={data.temp > 65 ? "text-danger" : ""}>
                {data.temp}°C
              </strong>
            </div>
          </div>

          {/* Memory Group */}
          <div className="telemetry-group">
            <div className="stat-row">
              <span>RAM / PSRAM (Free)</span>
              <strong>{toMB(data.ram_free)} / {toMB(data.psram_free)} MB</strong>
            </div>
            <div className="stat-row">
              <span>Stack Monitor</span>
              <strong>{data.stack_mon} B</strong>
            </div>
          </div>

          {/* Network & Uptime Group */}
          <div className="telemetry-group">
            <div className="stat-row">
              <span>WiFi Signal</span>
              <strong className={data.rssi < -70 ? "text-danger" : ""}>
                {data.rssi} dBm
              </strong>
            </div>
            <div className="stat-row">
              <span>System Uptime</span>
              <strong>{Math.floor(data.uptime / 60)} min</strong>
            </div>
          </div>
        </div>
      ) : (
        /* UI State when stream is loading or stopped */
        <div className="telemetry-idle">
          <p>{isActive ? "Connecting to stream..." : "Telemetry waiting for activation"}</p>
        </div>
      )}

      <div className="telemetry-controls">
        {!isActive ? (
          <button className="btn-primary" onClick={() => toggleTelemetry(true)}>
            Enable Telemetry
          </button>
        ) : (
          <button className="btn-stop-telemetry" onClick={() => toggleTelemetry(false)}>
            Disable Telemetry
          </button>
        )}
      </div>
    </div>
  );
};

export default Telemetry;