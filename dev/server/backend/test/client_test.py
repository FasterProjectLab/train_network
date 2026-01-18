import asyncio
import websockets
import os

async def test():
    # On récupère l'URL via une variable d'environnement ou localhost par défaut
    uri = os.getenv("WS_URL", "ws://localhost:8080/ws")
    print(f"Connexion à {uri}...")
    
    try:
        async with websockets.connect(uri) as ws:
            message = "Test depuis le container client"
            await ws.send(message)
            print(f"Envoyé : {message}")
            
            reponse = await ws.recv()
            print(f"Reçu du serveur : {reponse}")
    except Exception as e:
        print(f"Erreur : {e}")

if __name__ == "__main__":
    asyncio.run(test())