#!/usr/bin/env python3
"""Test DeathLink on your own: send deaths to your game, and see the ones it sends.

    python deathlink_tester.py --server archipelago.gg:38281 --slot Hinaloth_RS64 [--password ...]

Connects to your slot as a second, chat-only connection with the DeathLink tag (your game stays connected too).
Press Enter to send a death (type a cause first if you like); "q" quits. Deaths from your game are printed as they
arrive. The seed needs death_link: true. Needs Python 3.10+ and `pip install websockets`.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import ssl
import sys
import time
import uuid

try:
    import websockets
except ImportError:
    sys.exit("needs the websockets package: pip install websockets")

GAME = "Star Wars Rogue Squadron"
SOURCE = "DeathLinkTester"


async def connect(server: str):
    if "://" in server:
        return await websockets.connect(server, max_size=None)
    try:
        return await websockets.connect(f"wss://{server}", max_size=None, ssl=ssl.create_default_context())
    except Exception:
        return await websockets.connect(f"ws://{server}", max_size=None)


async def run(args: argparse.Namespace) -> None:
    ws = await connect(args.server)
    hello = json.loads(await ws.recv())
    if hello[0].get("cmd") != "RoomInfo":
        sys.exit(f"unexpected first packet: {hello}")
    await ws.send(json.dumps([{
        "cmd": "Connect", "game": GAME, "name": args.slot, "password": args.password, "uuid": str(uuid.uuid4()),
        "version": {"major": 0, "minor": 6, "build": 8, "class": "Version"},
        "items_handling": 0, "tags": ["DeathLink", "TextOnly"], "slot_data": False,
    }]))
    reply = json.loads(await ws.recv())
    if reply[0].get("cmd") != "Connected":
        sys.exit(f"connection refused: {reply[0].get('errors', reply)}")
    print(f"Connected to {args.server} as {args.slot} (DeathLink tag on).")
    print("Press Enter to send a death, or type a cause and press Enter. 'q' quits.\n")

    async def listen() -> None:
        async for raw in ws:
            for packet in json.loads(raw):
                if packet.get("cmd") == "Bounced" and "DeathLink" in packet.get("tags", []):
                    d = packet.get("data", {})
                    if d.get("source") != SOURCE:
                        print(f"<- death from {d.get('source')}: {d.get('cause', '')}")

    async def send() -> None:
        loop = asyncio.get_running_loop()
        while True:
            line = (await loop.run_in_executor(None, sys.stdin.readline)).strip()
            if line.lower() in ("q", "quit", "exit"):
                return
            cause = line or f"{SOURCE} pressed the button"
            await ws.send(json.dumps([{"cmd": "Bounce", "tags": ["DeathLink"],
                                       "data": {"time": time.time(), "source": SOURCE, "cause": cause}}]))
            print(f"-> death sent: {cause}")

    listener = asyncio.create_task(listen())
    await send()
    listener.cancel()
    await ws.close()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--server", required=True, help="e.g. archipelago.gg:38281 or ws://localhost:38281")
    ap.add_argument("--slot", required=True, help="your slot name")
    ap.add_argument("--password", default="", help="room password, if any")
    asyncio.run(run(ap.parse_args()))


if __name__ == "__main__":
    main()
