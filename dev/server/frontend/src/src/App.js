import React, { useState, useRef, useEffect } from 'react';
import './App.css';

function App() {
  const [myId, setMyId] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const [users, setUsers] = useState([]);
  const [targetId, setTargetId] = useState('');
  const [statusLogs, setStatusLogs] = useState([]);
  
  const socket = useRef(null);
  const pingInterval = useRef(null);
  const canvasRef = useRef(null);

  // --- GESTION DU FLUX VIDÉO (CANVAS) ---
  const renderImage = (blob) => {
    const ctx = canvasRef.current?.getContext('2d');
    if (!ctx) return;

    const img = new Image();
    const url = URL.createObjectURL(blob);

    img.onload = () => {
      // Ajuste le canvas à la taille de l'image reçue (VGA par défaut)
      if (canvasRef.current.width !== img.width) {
        canvasRef.current.width = img.width;
        canvasRef.current.height = img.height;
      }
      ctx.drawImage(img, 0, 0);
      URL.revokeObjectURL(url); // Libération immédiate de la mémoire
    };
    img.src = url;
  };

  // --- HEARTBEAT ---
  useEffect(() => {
    if (isConnected) {
      pingInterval.current = setInterval(() => {
        if (socket.current?.readyState === WebSocket.OPEN) {
          socket.current.send(JSON.stringify({ type: "ping" }));
        }
      }, 5000);
    }
    return () => { if (pingInterval.current) clearInterval(pingInterval.current); };
  }, [isConnected]);

  // --- CONNEXION ---
  const connect = () => {
    if (!myId) return;
    socket.current = new WebSocket(`ws://localhost:8080/ws/${myId}`);
    socket.current.binaryType = 'blob'; // Crucial pour recevoir le flux de l'ESP32

    socket.current.onopen = () => {
      setIsConnected(true);
      socket.current.send(JSON.stringify({ type: "get-clients" }));
      addLog("Connecté en tant que : " + myId);
    };

    socket.current.onmessage = (event) => {
      // 1. Gestion du binaire (Vidéo)
      if (event.data instanceof Blob) {
        renderImage(event.data);
        return;
      }

      // 2. Gestion du texte (JSON)
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === "ping") return;

        if (msg.type === "users-list") {
          setUsers(msg.users.filter(u => u !== myId));
        } else {
          addLog(`De [${msg.from}]: ${JSON.stringify(msg.data)}`);
        }
      } catch (e) {
        console.error("Erreur JSON", e);
      }
    };

    socket.current.onclose = () => {
      setIsConnected(false);
      addLog("⚠️ Déconnecté");
    };
  };

  // --- ACTIONS ---
  const addLog = (text) => {
    setStatusLogs(prev => [new Date().toLocaleTimeString() + " - " + text, ...prev].slice(0, 10));
  };

  const sendCommand = (type, data = "") => {
    if (socket.current?.readyState === WebSocket.OPEN && targetId) {
      socket.current.send(JSON.stringify({ to: targetId, type, data }));
      addLog(`🚀 Ordre ${type} envoyé à ${targetId}`);
    }
  };

  const refreshUsers = () => {
    if (socket.current?.readyState === WebSocket.OPEN) {
      socket.current.send(JSON.stringify({ type: "get-clients" }));
    }
  };

  if (!isConnected) {
    return (
      <div className="login-container">
        <div className="login-card">
          <h2>Admin Panel</h2>
          <input 
            placeholder="Admin ID (ex: PC-Control)" 
            value={myId} 
            onChange={e => setMyId(e.target.value)} 
            onKeyDown={e => e.key === 'Enter' && connect()}
          />
          <button onClick={connect} className="refresh-btn">Se connecter</button>
        </div>
      </div>
    );
  }

  return (
    <div className="app-container">
      <div className="sidebar">
        <button onClick={refreshUsers} className="refresh-btn">🔄 Actualiser Liste</button>
        <h3>Appareils</h3>
        <div className="user-list">
          {users.map(u => (
            <div key={u} className={`user-item ${targetId === u ? 'active' : ''}`} onClick={() => setTargetId(u)}>
              <span className="status-dot"></span> {u}
            </div>
          ))}
        </div>
      </div>

      <div className="control-panel">
        {targetId ? (
          <div className="panel-content">
            <header className="panel-header">
              <h2>Contrôle : <span className="highlight">{targetId}</span></h2>
            </header>
            
            <div className="actions-section" style={{ display: 'flex', gap: '20px', flexWrap: 'wrap' }}>
              {/* Vidéo Live */}
              <div className="card" style={{ flex: '2', minWidth: '400px' }}>
                <h3>Flux Vidéo Temps Réel</h3>
                <canvas ref={canvasRef} style={{ width: '100%', background: '#000', borderRadius: '5px' }} />
                <div style={{ display: 'flex', gap: '10px', marginTop: '10px' }}>
                  <button onClick={() => sendCommand("push", "start camera")} style={{ background: '#27ae60', color: 'white', padding: '10px', border: 'none', borderRadius: '5px', cursor: 'pointer', flex: 1 }}>Démarrer</button>
                  <button onClick={() => sendCommand("push", "stop camera")} style={{ background: '#7f8c8d', color: 'white', padding: '10px', border: 'none', borderRadius: '5px', cursor: 'pointer', flex: 1 }}>Arrêter</button>
                </div>
              </div>

              {/* Maintenance */}
              <div className="card" style={{ flex: '1', minWidth: '250px' }}>
                <h3>Maintenance</h3>
                <button onClick={() => sendCommand("push", "update firmware")} className="ota-btn">OTA Update</button>
              </div>
            </div>

            <div className="logs-section" style={{ marginTop: '20px' }}>
              <h3>Console Système</h3>
              <div className="log-window">
                {statusLogs.map((log, i) => <div key={i} className="log-entry">{log}</div>)}
              </div>
            </div>
          </div>
        ) : (
          <div className="panel-content">Sélectionnez un appareil pour commencer...</div>
        )}
      </div>
    </div>
  );
}

export default App;