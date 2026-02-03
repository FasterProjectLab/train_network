import React, { useState } from 'react';
import './App.css';

import { useControlLogic } from './hooks/useControlLogic';
import Login from './components/login/Login';
import PanelControl from './components/panelControl/PanelControl';

/**
 * @brief Root Application Component
 * * This component acts as the main orchestrator for the train control interface.
 * It handles the high-level routing between the authentication (Login) phase 
 * and the main dashboard (PanelControl) based on the WebSocket connection state.
 */
const App: React.FC = () => {
  // Local state for identifying the controller and the currently selected train
  const [myId, setMyId] = useState<string>('');
  const [targetId, setTargetId] = useState<string>('');

  /**
   * We extract the business logic and WebSocket state from our custom hook.
   * This keeps the UI components clean and focused only on rendering.
   */
  const { 
    isConnected,      // Boolean: True if the WebSocket connection is active
    trains,           // String Array: List of available trains discovered by the server
    statusLogs,       // String Array: History of system events and commands
    addLog,           // Function: Allows UI components to push new messages to the log
    handleConnect     // Function: Triggers the WebSocket handshake process
  } = useControlLogic(myId);

  return (
    <div className="main-wrapper">
      {/* Conditional View Logic:
          If not connected, show the Login screen.
          Once connection is established (isConnected === true), show the Control Panel.
      */}
      {!isConnected ? (
        <Login 
          myId={myId} 
          setMyId={setMyId} 
          connect={handleConnect} 
        />
      ) : (
        <PanelControl 
          myId={myId} 
          trains={trains}
          targetId={targetId}
          setTargetId={setTargetId}
          statusLogs={statusLogs} 
          addLog={addLog}
        />
      )}
    </div>
  );
}

export default App;