# Mini Globed — Guida Completa

## Cos'è Mini Globed?

Mini Globed è una mod multiplayer **semplificata e didattica** per Geometry Dash, creata con Geode SDK.
Ti permette di vedere altri giocatori nello stesso livello in tempo reale.

**Versioni supportate:**
- **PC Windows**: Geode 5.3.0 + GD 2.2081
- **Android**: Geode + GD 2.2074 (APK)

> ⚠️ **NOTA IMPORTANTE**: Questo è un progetto didattico/sperimentale! Non è un sostituto di Globed.
> Mancano molte funzionalità avanzate come: autenticazione, crittografia, sincronizzazione perfetta,
> supporto a tutte le icone, ecc.

---

## Struttura del Progetto

```
mini-globed/
├── mod/                    ← La mod Geode (C++)
│   ├── CMakeLists.txt      ← File di configurazione build
│   ├── mod.json            ← Metadati della mod
│   └── src/
│       ├── main.cpp        ← File principale con gli hook
│       ├── network.hpp     ← Header del sistema di rete
│       ├── network.cpp     ← Implementazione rete (UDP)
│       ├── player_node.hpp ← Header dei giocatori remoti
│       └── player_node.cpp ← Rendering giocatori remoti
├── server/
│   └── server.py           ← Server multiplayer (Python)
└── GUIDA.md                ← Questo file
```

---

## PARTE 1: Avviare il Server

Il server è scritto in Python e non richiede librerie esterne.

### Requisiti
- Python 3.8 o superiore

### Avvio
```bash
cd server
python server.py
```

Vedrai:
```
╔══════════════════════════════════════════╗
║        Mini Globed Server v1.0           ║
╠══════════════════════════════════════════╣
║  Indirizzo: 0.0.0.0:4747                ║
║  In attesa di giocatori...               ║
╚══════════════════════════════════════════╝
```

### Opzioni
```bash
python server.py --port 5000        # Usa una porta diversa
python server.py --host 192.168.1.5 # Ascolta su un IP specifico
```

### Per giocare con amici su internet
1. Apri la porta 4747 (UDP) nel tuo router (port forwarding)
2. Dai il tuo IP pubblico ai tuoi amici (cercalo su whatismyip.com)
3. I tuoi amici inseriscono il tuo IP nelle impostazioni della mod

---

## PARTE 2: Compilare la Mod (PC Windows — GD 2.2081)

### Requisiti
1. **Visual Studio 2022** con il workload "Sviluppo di applicazioni desktop con C++"
2. **CMake** (versione 3.21+) — scaricabile da cmake.org
3. **Geode CLI** (v5.3.0) — scaricabile da [geode-sdk.org](https://geode-sdk.org)
4. **Geode SDK** (v5.3.0) — installato tramite Geode CLI
5. **C++23** — richiesto da Geode 5.x (Visual Studio 2022 lo supporta)

### Installare Geode CLI e SDK
```powershell
# Dopo aver installato Geode CLI, apri PowerShell e scrivi:
geode sdk install

# Imposta la variabile d'ambiente GEODE_SDK
# (Geode CLI dovrebbe farlo automaticamente)
```

### Compilare
```powershell
cd mod

# Crea la cartella di build
mkdir build
cd build

# Configura il progetto
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compila
cmake --build . --config Release
```

Se tutto va bene, troverai un file `.geode` nella cartella build.

### Installare la mod
```powershell
# Copia il file .geode nella cartella mods di Geode
copy MiniGlobed.geode "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods\"
```

---

## PARTE 2B: Compilare per Android (GD 2.2074)

La compilazione per Android è più complessa e richiede:
1. **Android NDK** (r26b consigliato)
2. **Geode CLI** configurato per cross-compilation
3. **GD 2.2074** installato su Android con il Geode Launcher

```bash
# Configurazione per Android (armv7 per GD 2.2074)
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
         -DANDROID_ABI=armeabi-v7a \
         -DANDROID_PLATFORM=android-24 \
         -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release
```

Il file `.geode` risultante va copiato nella cartella mods di Geode su Android
(di solito `/sdcard/Android/data/com.robtopx.geometryjump/geode/mods/`).

> **NOTA**: Su Android con GD 2.2074, assicurati di avere la versione di Geode
> compatibile (4.x). Geode 5.x è per GD 2.2081. Il Geode Launcher per Android
> gestisce automaticamente la versione corretta.

---

## PARTE 3: Configurazione

### Nelle impostazioni della mod (dentro GD)
1. Apri Geometry Dash
2. Vai in Geode → Mini Globed → Impostazioni
3. Configura:
   - **Server IP**: `127.0.0.1` per giocare in locale, o l'IP dell'amico che hosta
   - **Server Port**: `4747` (o quello scelto)
   - **Nome giocatore**: il tuo nome

---

## PARTE 4: Come Funziona (per imparare!)

### Il flusso base:
1. Entri in un livello → la mod si connette al server e invia un pacchetto JOIN
2. Ogni ~33ms → la mod invia la tua posizione (pacchetto UPDATE)
3. Il server riceve → inoltra a tutti gli altri giocatori
4. La mod riceve posizioni altrui → crea/aggiorna sprite nel livello
5. Esci dal livello → pacchetto LEAVE, disconnessione

### Concetti importanti che imparerai:
- **Socket UDP**: comunicazione veloce senza conferme
- **Serializzazione**: convertire strutture C++ in bytes da inviare in rete
- **Interpolazione (lerp)**: rendere il movimento fluido nonostante i pacchetti arrivino a intervalli
- **Hook/Patching**: modificare il comportamento di un programma senza il codice sorgente
- **Singleton Pattern**: una sola istanza del NetworkManager per tutta la mod
- **Multi-threading**: il thread di ricezione gira separatamente dal gioco

---

## Problemi Comuni

| Problema | Soluzione |
|----------|-----------|
| Non si connette | Verifica che il server sia avviato e l'IP/porta siano corretti |
| Non vedo altri giocatori | Controlla che siate nello stesso livello |
| Lag/giocatori che saltano | Normale su connessioni lente, prova a ridurre SEND_INTERVAL |
| Crash all'avvio | Verifica la versione di Geode e GD |

---

## Prossimi Passi (per migliorare la mod)

Se vuoi continuare a sviluppare, ecco alcune idee in ordine di difficoltà:

1. **Facile**: Aggiungi colori personalizzati per ogni giocatore
2. **Medio**: Carica le icone vere dei giocatori (cubo, nave, ecc.)
3. **Medio**: Aggiungi una chat testuale
4. **Difficile**: Sincronizza anche gli effetti (morte, respawn, portali)
5. **Molto difficile**: Aggiungi autenticazione e crittografia
6. **Esperto**: Sincronizza la percentuale di completamento e la classifica

Buon divertimento e buono studio! 🎮
