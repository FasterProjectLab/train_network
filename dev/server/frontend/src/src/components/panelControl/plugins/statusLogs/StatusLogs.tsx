import React, { useEffect, useRef } from 'react';
import './StatusLogs.css';

/**
 * @interface StatusLogsProps
 * @brief Defines the component properties for the system log viewer.
 */
interface StatusLogsProps {
  logs: string[];
}

/**
 * @component StatusLogs
 * @brief A real-time console component that displays system events with automatic scrolling.
 */
const StatusLogs: React.FC<StatusLogsProps> = ({ logs }) => {
  // Ref typed for an HTML div element to manage manual scroll manipulation
  const scrollRef = useRef<HTMLDivElement | null>(null);

  /**
   * @note Auto-scroll Logic
   * Whenever the logs array updates, scroll to the bottom of the container 
   * to ensure the most recent message is visible.
   */
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [logs]);

  return (
    <div className="logs-section">
      <h3>System Console</h3>
      <div className="log-window" ref={scrollRef}>
        {logs.length > 0 ? (
          logs.map((log, i) => {
            // Safe extraction of timestamp and log content
            let time: string;
            let message: string;

            // Check if the log follows the "Time - Message" format
            if (log.includes(' - ')) {
              const [extractedTime, ...messageParts] = log.split(' - ');
              time = extractedTime;
              // Rejoin remaining parts in case the message itself contains " - "
              message = messageParts.join(' - '); 
            } else {
              // Fallback if the log format is unexpected
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
          /* Placeholder displayed when the log history is empty */
          <div className="log-entry empty">Waiting for system data...</div>
        )}
      </div>
    </div>
  );
};

export default StatusLogs;