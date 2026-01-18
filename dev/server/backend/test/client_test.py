import asyncio
import websockets
import json
import os

async def listen():
    # On se connecte en tant que "test1" pour recevoir des messages
    uri = "ws://localhost:8080/ws/test1"
    async with websockets.connect(uri) as ws:
        print("Client 'test1' en attente de messages...")
        async for message in ws:
            print(f"Message reçu par test1 : {message}")

async def send():
    # On se connecte en tant que "expediteur"
    uri = "ws://localhost:8080/ws/expediteur"
    await asyncio.sleep(1) # Attendre que test1 soit prêt
    async with websockets.connect(uri) as ws:
        payload = {
            "to": "test1",
            "data": {"status": "ok", "msg": "Bonjour test1 !"}
        }
        await ws.send(json.dumps(payload))
        print(f"Message envoyé à test1")
        await asyncio.sleep(1)

async def main():
    # On lance les deux en parallèle pour le test
    await asyncio.gather(listen(), send())

if __name__ == "__main__":
    asyncio.run(main())