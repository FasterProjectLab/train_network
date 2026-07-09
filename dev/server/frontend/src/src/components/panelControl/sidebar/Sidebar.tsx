import React from 'react';
import './Sidebar.css';
import { Train } from '../../../models/Protocol';
import wsService from '../../../services/WebsocketService';

/**
 * @interface SidebarProps
 * @brief Defines the properties required for the Sidebar navigation component.
 */
interface SidebarProps {
  trains: Train[];                // List of active train IDs (from WebSocket state)
  targetTrainId: string | null;                // Currently selected train ID
  setTargetTrainId: (id: string) => void; // Function to update the selected target
  refreshTrains: () => void;       // Function to trigger a manual train list request
}

/**
 * @component Sidebar
 * @brief Navigation component that displays the list of connected devices.
 * Allows the user to switch between different trains and refresh the discovery list.
 */
const Sidebar: React.FC<SidebarProps> = ({ 
  trains = [], 
  targetTrainId, 
  setTargetTrainId, 
  refreshTrains,
}) => {

  const handleSelectTrain = (newTrainId: string) => {
    if (newTrainId === targetTrainId) return;
    
    setTargetTrainId(newTrainId);
  };

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
              key={t.id} 
              className={`user-item ${targetTrainId === t.id ? 'active' : ''}`} 
              onClick={() => handleSelectTrain(t.id)}
            >
              {/* Visual indicator for online status */}
              <span className="status-dot"></span> 
              {t.id}
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