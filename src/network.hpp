#pragma once

// ============================================================
// network.hpp — Gestione connessione UDP al server
// ============================================================
// Questo file gestisce la comunicazione di rete tra il client
// (la mod dentro GD) e il server Python.
// Usiamo UDP perché è veloce (non aspetta conferme come TCP),
// perfetto per mandare posizioni in tempo reale.
// ============================================================

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SocketType = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using SocketType = int;
    #define INVALID_SOCK -1
#endif

// Struttura che rappresenta lo stato di un giocatore
// Viene inviata al server e ricevuta dagli altri giocatori
struct PlayerData {
    uint32_t playerId;      // ID unico del giocatore
    char name[32];          // Nome del giocatore
    float posX;             // Posizione X nel livello
    float posY;             // Posizione Y nel livello
    float rotation;         // Rotazione del cubo/navicella
    uint8_t gameMode;       // 0=cubo, 1=nave, 2=palla, ecc.
    bool isPlayer2;         // Se è il secondo giocatore (dual mode)
    bool isDead;            // Se il giocatore è morto
    uint32_t levelId;       // ID del livello in cui si trova
    float iconScale;        // Scala dell'icona
};

// Tipi di pacchetto che possiamo inviare/ricevere
enum class PacketType : uint8_t {
    JOIN = 1,       // Un giocatore si connette
    LEAVE = 2,      // Un giocatore si disconnette  
    UPDATE = 3,     // Aggiornamento posizione
    PING = 4,       // Controllo connessione
    PONG = 5,       // Risposta al ping
};

// Struttura di un pacchetto di rete
struct Packet {
    PacketType type;
    PlayerData data;
};

class NetworkManager {
public:
    // Singleton — una sola istanza in tutto il programma
    static NetworkManager& get();

    // Connessione al server
    bool connect(const std::string& ip, int port);
    void disconnect();
    bool isConnected() const;

    // Invio dati
    void sendPlayerUpdate(const PlayerData& data);
    void sendJoin(const PlayerData& data);
    void sendLeave();

    // Ricezione — chiama il callback quando arrivano dati
    void setOnPlayerUpdate(std::function<void(const PlayerData&)> callback);
    void setOnPlayerLeave(std::function<void(uint32_t)> callback);

    // Lista giocatori attualmente connessi
    std::vector<PlayerData> getOtherPlayers();

private:
    NetworkManager() = default;
    ~NetworkManager();

    void receiveLoop();     // Thread che ascolta i pacchetti in arrivo

    SocketType m_socket = INVALID_SOCK;
    sockaddr_in m_serverAddr{};
    std::atomic<bool> m_connected{false};
    std::thread m_recvThread;

    std::mutex m_playersMutex;
    std::vector<PlayerData> m_otherPlayers;

    std::function<void(const PlayerData&)> m_onUpdate;
    std::function<void(uint32_t)> m_onLeave;
};
