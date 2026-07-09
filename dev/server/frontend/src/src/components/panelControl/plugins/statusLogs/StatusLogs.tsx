import React, { useEffect, useRef } from 'react';
import './StatusLogs.css';

interface StatusLogsProps {
  logs: string[];
}

const StatusLogs: React.FC<StatusLogsProps> = ({ logs }) => {
  const scrollRef = useRef<HTMLDivElement | null>(null);

  // Aligne le scroll tout en bas à chaque mise à jour du tableau de logs
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTo({
        top: scrollRef.current.scrollHeight,
        behavior: 'smooth' // Optionnel : rend le défilement fluide
      });
    }
  }, [logs]);

  return (
    <div className="logs-section">
      <h3>System Console</h3>
      <div className="log-window" ref={scrollRef}>
        {logs.length > 0 ? (
          logs.map((log, i) => {
            let time: string;
            let message: string;

            if (log.includes(' - ')) {
              const [extractedTime, ...messageParts] = log.split(' - ');
              time = extractedTime;
              message = messageParts.join(' - '); 
            } else {
              time = new Date().toLocaleTimeString();
              message = log;
            }
            
            return (
              <div key={i} className="log-entry">
                <span className="log-time">[{time}]</span>
                <span className="log-text">{message}</span>
              </div>
            );
          })
        ) : (
          <div className="log-entry empty">Waiting for system data...</div>
        )}
      </div>
    </div>
  );
};

export default StatusLogs;