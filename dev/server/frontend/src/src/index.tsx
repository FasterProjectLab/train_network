import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import App from './App';
import reportWebVitals from './reportWebVitals';

/**
 * @brief Application Entry Point
 * * This script initializes the React application by mounting the root component
 * to the DOM element with the ID 'root'.
 */
const root = ReactDOM.createRoot(
  document.getElementById('root') as HTMLElement
);

root.render(
  /**
   * @note React.StrictMode
   * Used during development to highlight potential problems in the application.
   * It renders components twice to detect side effects.
   */
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

/**
 * @brief Performance Monitoring
 * If you want to start measuring performance in your app, pass a function
 * to log results (for example: reportWebVitals(console.log))
 * or send to an analytics endpoint.
 */
reportWebVitals();