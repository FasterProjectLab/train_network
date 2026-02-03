import React from 'react';
import './Sidebar.css';

/**
 * @interface SidebarProps
 * @brief Defines the properties required for the Sidebar navigation component.
 */
interface SidebarProps {
  trains: string[];                // List of active train IDs (from WebSocket state)
  targetId: string;                // Currently selected train ID
  setTargetId: (id: string) => void; // Function to update the selected target
  refreshTrains: () => void;       // Function to trigger a manual train list request
}

/**
 * @component Sidebar
 * @brief Navigation component that displays the list of connected devices.
 * Allows the user to switch between different trains and refresh the discovery list.
 */
const Sidebar: React.FC<SidebarProps> = ({ 
  trains = [], 
  targetId, 
  setTargetId, 
  refreshTrains,
}) => {
  return (
    <aside className="sidebar">
      {/* Manual refresh button to poll the server for active sessions */}
      <button onClick={refreshTrains} className="btn-sidebar-refresh">
        Refresh Device List
      </button>
      
      <h3>Connected Devices</h3>
      
      <div className="user-list">
        {trains.length > 0 ? (
          trains.map((t) => (
            <div 
              key={t} 
              className={`user-item ${targetId === t ? 'active' : ''}`} 
              onClick={() => setTargetId(t)}
            >
              {/* Visual indicator for online status */}
              <span className="status-dot"></span> 
              {t}
            </div>
          ))
        ) : (
          /* Placeholder displayed when the train list is empty */
          <div className="no-users">No devices detected</div>
        )}
      </div>
    </aside>
  );
};

export default Sidebar;