import React, { useState, useEffect, useCallback } from 'react';
import wsService from '../../../../services/WebsocketService';
import { ProtocolConstants } from '../../../../models/Protocol';
import './Telemetry.css';

interface TelemetryViewProps {
  targetId: string;
}

interface TelemetryData {
  cpu0: number;
  cpu1: number;
  temp: number;
  ram_free: number;
  ram_min: number;
  psram_free: number;
  rssi: number;
  wifi_std: string;
  wifi_chan: number;
  wifi_bw_real: number; 
  uptime: number;
  stack_mon: number; 
}

const Telemetry: React.FC<TelemetryViewProps> = ({ targetId }) => {
  const [data, setData] = useState<TelemetryData | null>(null);
  const [isActive, setIsActive] = useState<boolean>(false);
  const [refreshInterval, setRefreshInterval] = useState<number>(500);

  const toggleTelemetry = useCallback((active: boolean) => {
    if (!targetId) return;
    wsService.send(JSON.stringify({
      type: active ? ProtocolConstants.ActionSubscibe : ProtocolConstants.ActionUnsubscribe,
      target: targetId,
      payload: { interval: refreshInterval },
      tag: "telemetry"
    }));
    setIsActive(active);
  }, [targetId, refreshInterval]);

  useEffect(() => {
    const handleTelemetry = (rawData: string | Blob) => {
      if (!isActive || typeof rawData !== 'string') return;
      try {
        const envelope = JSON.parse(rawData);
        if (envelope.type === "telemetry" && envelope.source === targetId) {
          setData(envelope.payload);
        }
      } catch (e) {}
    };
    wsService.addListener(handleTelemetry);
    return () => {
      wsService.removeListener(handleTelemetry);
      if (isActive) {
        wsService.send(JSON.stringify({
          type: ProtocolConstants.ActionUnsubscribe,
          target: targetId,
          payload: "telemetry"
        }));
      }
    };
  }, [targetId, isActive]);

  const toMB = (bytes: number) => (bytes / (1024 * 1024)).toFixed(2);
  const toKB = (bytes: number) => (bytes / 1024).toFixed(1);

  const formatBW = (bw: number) => {
    if (bw === 40) return "40 MHz (+)";
    if (bw === -40) return "40 MHz (-)";
    return "20 MHz";
  };

  return (
    <div className="card telemetry-card">
      <div className="card-header">
        <h3>System Metrics</h3>
        <div className="header-controls">
          <div className="refresh-input-group">
            <input type="number" value={refreshInterval} onChange={(e) => setRefreshInterval(Number(e.target.value))} disabled={isActive} min="500" step="100" />
            <span>ms</span>
          </div>
          <span className={`status-indicator ${isActive ? 'online' : 'offline'}`}>{isActive ? "Live Feed" : "Feed Inactive"}</span>
        </div>
      </div>

      {isActive && data ? (
        <div className="telemetry-grid">
          {/* SoC & Processing */}
          <div className="telemetry-group">
            <h4>Processing & Thermal</h4>
            <div className="stat-row"><span>CPU Load</span><strong>{data.cpu0}% | {data.cpu1}%</strong></div>
            <div className="stat-row"><span>SoC Temp</span><strong className={data.temp > 65 ? "text-danger" : ""}>{data.temp}°C</strong></div>
            <div className="stat-row"><span>Uptime</span><strong>{Math.floor(data.uptime / 60)} min</strong></div>
          </div>

          {/* Memory Management - Regroupé ici */}
          <div className="telemetry-group">
            <h4>Memory (Heap & Stack)</h4>
            <div className="stat-row">
              <span>Internal RAM</span><strong>{toMB(data.ram_free)} MB</strong>
              <small title="Minimum free RAM recorded since boot" className="text-muted">
                  (Min: {toMB(data.ram_min)} MB)
              </small>
            </div>
            <div className="stat-row">
              <span>External PSRAM</span><strong>{toMB(data.psram_free)} MB</strong>
            </div>
            <div className="stat-row highlight">
              <span title="Minimum stack space ever available since task start">Stack Margin (HWM)</span>
              <strong className={data.stack_mon < 512 ? "text-danger" : ""}>{toKB(data.stack_mon)} KB</strong>
            </div>
          </div>

          {/* Network Details */}
          <div className="telemetry-group">
            <h4>Network Info ({data.wifi_std})</h4>
            <div className="stat-row"><span>Signal RSSI</span><strong className={data.rssi < -70 ? "text-danger" : ""}>{data.rssi} dBm</strong></div>
            <div className="stat-row"><span>Channel / BW</span><strong>Ch {data.wifi_chan} @ {formatBW(data.wifi_bw_real)}</strong></div>
          </div>
        </div>
      ) : (
        <div className="telemetry-idle">
          <p>{isActive ? "Connecting to stream..." : "Telemetry waiting for activation"}</p>
        </div>
      )}

      <div className="telemetry-controls">
        <button className={!isActive ? "btn-primary" : "btn-stop-telemetry"} onClick={() => toggleTelemetry(!isActive)}>
          {!isActive ? "Enable Telemetry" : "Disable Telemetry"}
        </button>
      </div>
    </div>
  );
};

export default Telemetry;