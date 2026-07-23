import React, { useEffect, useRef, useState } from 'react';
import wsService from '../../../../services/WebsocketService'; 
import './VideoCanvas.css';

/**
 * @interface VideoCanvasProps
 * @brief Properties required for the Video Streaming component.
 */
interface VideoCanvasProps {
  myId: string;
  targetTrainId: string;
}

/**
 * @constant resolutions
 * @brief Maps user-friendly labels to ESP32-CAM 'framesize' sensor values.
 */
const resolutions = [
  { label: 'QVGA', value: 6 },
  { label: 'VGA', value: 10 },
  { label: 'SVGA', value: 11 },
  { label: 'XGA', value: 12 },
];

/**
 * @component VideoCanvas
 * @brief Component responsible for displaying the MJPEG stream and controlling camera sensor settings.
 */
const VideoCanvas: React.FC<VideoCanvasProps> = ({ myId, targetTrainId }) => {
  
  const [src, setSrc] = useState<string | undefined>(undefined);
  const [currentRes, setCurrentRes] = useState(10); // Default: VGA
  const [quality, setQuality] = useState(10);      // Lower value = Higher quality in JPEG
  const [isStreaming, setIsStreaming] = useState<boolean>(false);      // Lower value = Higher quality in JPEG
  const currentTargetId = useRef<string | null>(null);
  const baseUrl = `http://localhost:8080/video/${currentTargetId.current}?userId=${myId}`;

  /**
   * @brief Generates a unique URL with a timestamp to bypass browser caching.
   */
  const handleRefresh = () => {
    const timestamp = new Date().getTime();

    setSrc(`${baseUrl}&t=${timestamp}`);
  };

  const onStart = () => {
    if (currentTargetId.current) {
      wsService.sendStartCamera(currentTargetId.current);
      setIsStreaming(true);
    }
  };

  const onStop = () => {

    if (currentTargetId.current) {
      wsService.sendStopCamera(currentTargetId.current);
      setIsStreaming(false);
    }
  };

  /**
   * @brief Sends a command to change JPEG compression quality.
   * @param val Integer (usually 10-63). Lower is better quality but higher bandwidth.
   */
  const changeQuality = (val: number) => {
    setQuality(val);
    if (isStreaming) {
      wsService.send(JSON.stringify({
        type: "camera_control",
        source: myId,
        target: currentTargetId.current,
        payload: { 
            variable: "quality", 
            value: val 
        }
      }));
    }
  };

  /**
   * @brief Changes the camera sensor resolution.
   * @param resId The framesize ID recognized by the ESP32-CAM driver.
   */
  const changeResolution = (resId: number) => {
    setCurrentRes(resId);
    if (isStreaming) {
      wsService.send(JSON.stringify({
        type: "camera_control",
        source: myId,
        target: currentTargetId.current,
        payload: { 
            variable: "framesize", 
            value: resId 
        }
      }));
      // Trigger a refresh after a short delay to allow the sensor to stabilize
      setTimeout(handleRefresh, 500);
    }
  };

  /**
   * @note Lifecycle Management
   * Automatically triggers the first frame load when streaming starts.
   */
  useEffect(() => {
    if (isStreaming) {
      // Delay slightly to ensure the server-side proxy is ready
      setTimeout(handleRefresh, 2000);
    } else {
      console.log("clean");
      setSrc(undefined);
    }
  }, [isStreaming]);

  useEffect(() => {
    onStop();
    currentTargetId.current = targetTrainId;
  }, [targetTrainId]);
    
  return (
    <div className="card video-container">
      <div className="video-header">
        <div className="header-info">
          <span className="dot"></span>
          <h3>Camera Feed</h3>
        </div>
        <div className="header-res-badge">
          {resolutions.find(r => r.value === currentRes)?.label}
        </div>
      </div>
        
      <div className="canvas-wrapper">
        {isStreaming &&
        <img 
          src={src} 
          alt="ESP32 Stream"
          className={`video-canvas ${!isStreaming ? 'blur' : ''}`} 
          // Auto-retry on stream interruption
          onError={() => {if (isStreaming) {setTimeout(handleRefresh, 2000)}}}
        />
        }
        
        {isStreaming && <div className="status-badge online overlay-badge">LIVE</div>}

        {/* Start Overlay displayed when stream is inactive */}
        {!isStreaming && (
          <div className="video-overlay">
            <div className="overlay-content">
              <button className="btn-primary start-pulse" onClick={onStart}>
                Start Video Stream
              </button>
            </div>
          </div>
        )}
      </div>

      <div className="video-controls">
        {/* Resolution Picker */}
        <div className="control-section">
          <label className="section-label">Resolution</label>
          <div className="resolution-picker-group">
            {resolutions.map((res) => (
              <button
                key={res.value}
                className={`btn-res-pill ${currentRes === res.value ? 'active' : ''}`}
                onClick={() => changeResolution(res.value)}
                disabled={!isStreaming}
              >
                {res.label}
              </button>
            ))}
          </div>
        </div>

        {/* Quality Slider */}
        <div className="control-section">
          <div className="label-row">
            <label className="section-label">Image Quality</label>
            <span className="value-indicator">{quality}</span>
          </div>
          <input 
            type="range" 
            min="8" 
            max="45" 
            value={quality} 
            onChange={(e) => changeQuality(parseInt(e.target.value))}
            disabled={!isStreaming}
            className="modern-slider"
          />
        </div>

        {/* Global Stream Actions */}
        <div className="main-actions-row">
            <button onClick={handleRefresh} className="btn-icon-text refresh" disabled={!isStreaming}>
                <span>🔄</span> Refresh Feed
            </button>
            <button className="btn-stop-camera-modern" onClick={onStop} disabled={!isStreaming}>
                Stop Stream
            </button>
        </div>
      </div>
    </div>
  );
};

export default VideoCanvas;