import React, { useEffect } from 'react';
import './Login.css';

/**
 * @interface LoginProps
 * @brief Defines the component properties for type safety.
 */
interface LoginProps {
  myId: string;
  setMyId: (id: string) => void;
  connect: () => void;
}

/**
 * @component Login
 * @brief Authentication view where the controller identifies themselves.
 */
const Login: React.FC<LoginProps> = ({ myId, setMyId, connect }) => {
  
  /**
   * @note Default Identity
   * Automatically sets a default ID if the field is empty on mount.
   */
  useEffect(() => {
    if (!myId) {
      setMyId('monitor');
    }
  }, [setMyId, myId]);

  return (
    <div className="login-container">
      <div className="login-card card">
        <h2>Admin Control Panel</h2>
        <input 
          placeholder="Controller ID (e.g., Admin-PC)" 
          value={myId} 
          // TypeScript infers 'e' as a React Change Event for HTMLInputElements
          onChange={(e: React.ChangeEvent<HTMLInputElement>) => setMyId(e.target.value)} 
          // Allows connection by pressing the Enter key
          onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => e.key === 'Enter' && connect()}
        />
        <button onClick={connect} className="btn-primary">
          Connect to System
        </button>
      </div>
    </div>
  );
};

export default Login;