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

  // --- GESTION DU HEARTBEAT (PING) ---
  useEffect(() => {
    if (isConnected) {
      pingInterval.current = setInterval(() => {
        if (socket.current?.readyState === WebSocket.OPEN) {
          socket.current.send(JSON.stringify({ type: "ping" }));
        }
      }, 5000);
    }

    return () => {
      if (pingInterval.current) clearInterval(pingInterval.current);
    };
  }, [isConnected]);

  // --- FONCTION DE CONNEXION ---
  const connect = () => {
    if (!myId) return;
    // Remplacez localhost par l'IP du serveur si nécessaire
    socket.current = new WebSocket(`ws://localhost:8080/ws/${myId}`);

    socket.current.onopen = () => {
      setIsConnected(true);
      socket.current.send(JSON.stringify({ type: "get-clients" }));
      addLog("Connecté au serveur en tant que : " + myId);
    };

    socket.current.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);

        // On ignore les pings qui pourraient revenir du serveur
        if (msg.type === "ping") return;

        if (msg.type === "users-list") {
          // Mise à jour de la liste des appareils (en excluant soi-même)
          setUsers(msg.users.filter(u => u !== myId));
        } else {
          // Log des messages reçus des ESP32 (ex: retours d'état)
          addLog(`Réception de [${msg.from}]: ${JSON.stringify(msg.data)}`);
        }
      } catch (e) {
        console.error("Erreur de parsing JSON", e);
      }
    };

    socket.current.onclose = () => {
      setIsConnected(false);
      addLog("⚠️ Déconnecté du serveur");
      if (pingInterval.current) clearInterval(pingInterval.current);
    };

    socket.current.onerror = (error) => {
      addLog("❌ Erreur WebSocket détectée");
      console.error(error);
    };
  };

  // --- ACTIONS ---
  const addLog = (text) => {
    setStatusLogs(prev => [new Date().toLocaleTimeString() + " - " + text, ...prev].slice(0, 10));
  };

  const sendUpdateFirmware = () => {
    if (socket.current?.readyState === WebSocket.OPEN && targetId) {
      const payload = { 
        to: targetId, 
        type: "push", 
        data: "update firmware" 
      };
      socket.current.send(JSON.stringify(payload));
      addLog(`🚀 Commande OTA envoyée à ${targetId}`);
    }
  };

  const refreshUsers = () => {
    if (socket.current?.readyState === WebSocket.OPEN) {
      socket.current.send(JSON.stringify({ type: "get-clients" }));
    }
  };

  // --- RENDU : ÉCRAN DE CONNEXION ---
  if (!isConnected) {
    return (
      <div className="login-container">
        <div className="login-card">
          <h2>Admin Panel</h2>
          <p>Entrez votre identifiant pour vous connecter au serveur.</p>
          <input 
            placeholder="Admin ID (ex: PC-Admin)" 
            value={myId} 
            onChange={e => setMyId(e.target.value)} 
            onKeyDown={e => e.key === 'Enter' && connect()}
          />
          <button onClick={connect}>Se connecter</button>
        </div>
      </div>
    );
  }

  // --- RENDU : DASHBOARD ---
  return (
    <div className="app-container">
      {/* Sidebar : Liste des appareils */}
      <div className="sidebar">
        <div className="sidebar-header">
          <h3>Appareils connectés</h3>
          <button onClick={refreshUsers} className="refresh-btn" title="Actualiser">🔄</button>
        </div>
        <div className="user-list">
          {users.length === 0 && <p className="empty-msg">Aucun train détecté...</p>}
          {users.map(u => (
            <div 
              key={u} 
              className={`user-item ${targetId === u ? 'active' : ''}`}
              onClick={() => setTargetId(u)}
            >
              <span className="status-dot"></span> {u}
            </div>
          ))}
        </div>
      </div>

      {/* Main Panel : Contrôle de l'appareil sélectionné */}
      <div className="control-panel">
        {targetId ? (
          <div className="panel-content">
            <header className="panel-header">
              <h2>ID Appareil : <span className="highlight">{targetId}</span></h2>
            </header>
            
            <div className="actions-section">
              <div className="card">
                <h3>Mise à jour (OTA)</h3>
                <p>Déclencher le téléchargement du nouveau firmware sur l'ESP32.</p>
                <button onClick={sendUpdateFirmware} className="ota-btn">
                  Lancer l'Update Firmware
                </button>
              </div>
            </div>

            <div className="logs-section">
              <h3>Derniers événements</h3>
              <div className="log-window">
                {statusLogs.map((log, i) => (
                  <div key={i} className="log-entry">{log}</div>
                ))}
                {statusLogs.length === 0 && <div className="log-placeholder">En attente de messages...</div>}
              </div>
            </div>
          </div>
        ) : (
          <div className="empty-state">
            <div className="empty-icon">🔌</div>
            <p>Sélectionnez un train dans la liste de gauche pour envoyer des commandes.</p>
          </div>
        )}
      </div>
    </div>
  );
}

export default App;