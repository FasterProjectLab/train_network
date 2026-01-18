import React, { useState, useRef } from 'react';
import './App.css';

function App() {
  const [myId, setMyId] = useState('');
  const [toId, setToId] = useState('');
  const [message, setMessage] = useState('');
  const [chat, setChat] = useState([]);
  const [isConnected, setIsConnected] = useState(false);
  const socket = useRef(null);

  const connect = () => {
    if (!myId) return;
    socket.current = new WebSocket(`ws://localhost:8080/ws/${myId}`);

    socket.current.onopen = () => {
      setIsConnected(true);
      addMessage({ system: true, text: `Connecté en tant que : ${myId}` });
    };

    socket.current.onmessage = (event) => {
      addMessage({ system: false, from: "Reçu", text: event.data, type: 'them' });
    };

    socket.current.onclose = () => setIsConnected(false);
  };

  const addMessage = (msgObj) => {
    setChat(prev => [...prev, msgObj]);
  };

  const sendMessage = (e) => {
    e.preventDefault();
    if (socket.current?.readyState === WebSocket.OPEN && toId && message) {
      const payload = { to: toId, data: message };
      socket.current.send(JSON.stringify(payload));
      addMessage({ system: false, from: "Moi", text: message, type: 'me' });
      setMessage('');
    }
  };

  return (
    <div className="chat-container">
      <h1>💬 WS Messenger</h1>

      {!isConnected ? (
        <div className="login-form">
          <input type="text" placeholder="Ton ID utilisateur..." value={myId} onChange={e => setMyId(e.target.value)} />
          <button onClick={connect}>Connexion</button>
        </div>
      ) : (
        <>
          <div className="status-header">● En ligne : {myId}</div>
          <form className="message-form" onSubmit={sendMessage}>
            <input type="text" placeholder="Destinataire (ID)" value={toId} onChange={e => setToId(e.target.value)} required />
            <textarea placeholder="Message..." value={message} onChange={e => setMessage(e.target.value)} required />
            <button type="submit">Envoyer</button>
          </form>
        </>
      )}

      <div className="chat-box">
        {chat.map((m, i) => (
          <div key={i} className={`msg-item ${m.system ? 'msg-system' : (m.type === 'me' ? 'msg-me' : 'msg-them')}`}>
            {!m.system && <b>{m.from}: </b>} {m.text}
          </div>
        ))}
      </div>
    </div>
  );
}

export default App;