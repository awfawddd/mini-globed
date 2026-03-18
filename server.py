#!/usr/bin/env python3
"""
============================================================
Mini Globed Server — Server multiplayer per Geometry Dash
============================================================
Questo è un server UDP semplice che:
1. Riceve i dati di posizione dai giocatori
2. Li inoltra a tutti gli altri giocatori connessi
3. Gestisce connessioni e disconnessioni

Per avviarlo: python server.py
Per cambiare porta: python server.py --port 4747

Requisiti: Python 3.8+ (nessuna libreria esterna necessaria!)
============================================================
"""

import socket
import struct
import time
import threading
import argparse
from dataclasses import dataclass
from typing import Dict, Tuple

# ============================================================
# Struttura pacchetti (deve corrispondere a quella C++)
# ============================================================

# Formato del pacchetto binario (deve matchare il C++)
# B = uint8 (PacketType)
# I = uint32 (playerId)
# 32s = char[32] (name)
# f = float (posX, posY, rotation)
# B = uint8 (gameMode)
# ? = bool (isPlayer2, isDead)
# I = uint32 (levelId)
# f = float (iconScale)
PACKET_FORMAT = '<B I 32s f f f B ? ? I f'
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)

# Tipi di pacchetto
PACKET_JOIN = 1
PACKET_LEAVE = 2
PACKET_UPDATE = 3
PACKET_PING = 4
PACKET_PONG = 5


@dataclass
class Player:
    """Informazioni su un giocatore connesso"""
    player_id: int
    name: str
    address: Tuple[str, int]  # (IP, porta)
    last_seen: float          # Timestamp ultimo pacchetto
    pos_x: float = 0.0
    pos_y: float = 0.0
    rotation: float = 0.0
    game_mode: int = 0
    is_player2: bool = False
    is_dead: bool = False
    level_id: int = 0
    icon_scale: float = 1.0


class MiniGlobedServer:
    def __init__(self, host: str = '0.0.0.0', port: int = 4747):
        self.host = host
        self.port = port
        self.players: Dict[Tuple[str, int], Player] = {}  # addr -> Player
        self.running = False
        self.sock = None
        self.cleanup_interval = 5.0   # Secondi tra ogni pulizia
        self.timeout = 10.0           # Secondi prima di considerare un giocatore disconnesso

    def start(self):
        """Avvia il server"""
        # Crea socket UDP
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.host, self.port))
        self.sock.settimeout(1.0)  # Timeout per non bloccarsi per sempre
        self.running = True

        print(f"""
╔══════════════════════════════════════════╗
║        Mini Globed Server v1.0           ║
╠══════════════════════════════════════════╣
║  Indirizzo: {self.host}:{self.port}            ║
║  In attesa di giocatori...               ║
║  Premi Ctrl+C per fermare                ║
╚══════════════════════════════════════════╝
        """)

        # Avvia thread di pulizia (rimuove giocatori inattivi)
        cleanup_thread = threading.Thread(target=self._cleanup_loop, daemon=True)
        cleanup_thread.start()

        # Loop principale di ricezione
        try:
            while self.running:
                try:
                    data, addr = self.sock.recvfrom(PACKET_SIZE + 16)  # Buffer extra
                    self._handle_packet(data, addr)
                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"[ERRORE] Ricezione: {e}")
        except KeyboardInterrupt:
            print("\n[INFO] Server in chiusura...")
        finally:
            self.running = False
            self.sock.close()
            print("[INFO] Server chiuso.")

    def _handle_packet(self, data: bytes, addr: Tuple[str, int]):
        """Gestisce un pacchetto ricevuto"""
        if len(data) < PACKET_SIZE:
            return  # Pacchetto troppo piccolo, ignora

        try:
            # Decodifica il pacchetto binario
            unpacked = struct.unpack(PACKET_FORMAT, data[:PACKET_SIZE])
            packet_type = unpacked[0]
            player_id = unpacked[1]
            name = unpacked[2].decode('utf-8', errors='ignore').rstrip('\x00')
            pos_x = unpacked[3]
            pos_y = unpacked[4]
            rotation = unpacked[5]
            game_mode = unpacked[6]
            is_player2 = unpacked[7]
            is_dead = unpacked[8]
            level_id = unpacked[9]
            icon_scale = unpacked[10]
        except struct.error as e:
            print(f"[ERRORE] Pacchetto malformato da {addr}: {e}")
            return

        # ---- Gestisci in base al tipo ----

        if packet_type == PACKET_JOIN:
            # Nuovo giocatore!
            player = Player(
                player_id=player_id,
                name=name,
                address=addr,
                last_seen=time.time(),
                pos_x=pos_x, pos_y=pos_y,
                rotation=rotation,
                game_mode=game_mode,
                level_id=level_id,
                icon_scale=icon_scale
            )
            self.players[addr] = player
            print(f"[+] {name} (ID:{player_id}) si è connesso da {addr[0]}:{addr[1]}")
            print(f"    Giocatori online: {len(self.players)}")

            # Invia a questo giocatore i dati di tutti gli altri
            for other_addr, other_player in self.players.items():
                if other_addr != addr:
                    self._send_player_data(addr, other_player, PACKET_JOIN)

            # Notifica tutti gli altri del nuovo giocatore
            self._broadcast(data, exclude=addr)

        elif packet_type == PACKET_UPDATE:
            # Aggiornamento posizione
            if addr in self.players:
                p = self.players[addr]
                p.last_seen = time.time()
                p.pos_x = pos_x
                p.pos_y = pos_y
                p.rotation = rotation
                p.game_mode = game_mode
                p.is_player2 = is_player2
                p.is_dead = is_dead
                p.level_id = level_id

                # Inoltra a tutti gli altri giocatori
                self._broadcast(data, exclude=addr)

        elif packet_type == PACKET_LEAVE:
            # Giocatore che se ne va
            if addr in self.players:
                player = self.players.pop(addr)
                print(f"[-] {player.name} si è disconnesso")
                print(f"    Giocatori online: {len(self.players)}")
                # Notifica gli altri
                self._broadcast(data, exclude=addr)

        elif packet_type == PACKET_PING:
            # Rispondi con PONG
            pong = bytearray(data)
            pong[0] = PACKET_PONG
            self.sock.sendto(bytes(pong), addr)

    def _broadcast(self, data: bytes, exclude: Tuple[str, int] = None):
        """Invia un pacchetto a tutti i giocatori tranne exclude"""
        for addr in self.players:
            if addr != exclude:
                try:
                    self.sock.sendto(data, addr)
                except Exception:
                    pass  # Ignora errori di invio

    def _send_player_data(self, to_addr: Tuple[str, int], player: Player, ptype: int):
        """Invia i dati di un giocatore specifico a un indirizzo"""
        name_bytes = player.name.encode('utf-8')[:31].ljust(32, b'\x00')
        packed = struct.pack(
            PACKET_FORMAT,
            ptype,
            player.player_id,
            name_bytes,
            player.pos_x,
            player.pos_y,
            player.rotation,
            player.game_mode,
            player.is_player2,
            player.is_dead,
            player.level_id,
            player.icon_scale
        )
        try:
            self.sock.sendto(packed, to_addr)
        except Exception:
            pass

    def _cleanup_loop(self):
        """Rimuove giocatori che non mandano pacchetti da troppo tempo"""
        while self.running:
            time.sleep(self.cleanup_interval)
            now = time.time()
            to_remove = []

            for addr, player in self.players.items():
                if now - player.last_seen > self.timeout:
                    to_remove.append(addr)
                    print(f"[TIMEOUT] {player.name} rimosso (inattivo)")

            for addr in to_remove:
                self.players.pop(addr, None)

            if to_remove:
                print(f"    Giocatori online: {len(self.players)}")


# ============================================================
# Avvio del server
# ============================================================
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Mini Globed Server')
    parser.add_argument('--host', default='0.0.0.0', help='Indirizzo di ascolto (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=4747, help='Porta (default: 4747)')
    args = parser.parse_args()

    server = MiniGlobedServer(host=args.host, port=args.port)
    server.start()
