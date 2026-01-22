import asyncio
import websockets
import cv2
import numpy as np

async def handle_esp32_stream(websocket):
    print(f"ESP32 connecté via {websocket.remote_address}")
    try:
        async for message in websocket:
            # Conversion des données binaires en image
            nparr = np.frombuffer(message, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is not None:
                cv2.imshow("Stream ESP32-CAM", frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                print("Erreur de décodage frame")
    except websockets.exceptions.ConnectionClosed:
        print("ESP32 déconnecté")
    finally:
        cv2.destroyAllWindows()

async def main():
    # On lance le serveur sur le port 8090
    async with websockets.serve(handle_esp32_stream, "0.0.0.0", 8080):
        print("Serveur WebSocket en attente sur le port 8080...")
        await asyncio.Future()  # Maintient le serveur ouvert indéfiniment

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServeur arrêté par l'utilisateur.")