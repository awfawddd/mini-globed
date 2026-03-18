// ============================================================
// network.cpp — Implementazione della rete
// ============================================================
// Qui c'è il codice vero e proprio che:
// 1. Apre un socket UDP
// 2. Invia pacchetti al server
// 3. Riceve pacchetti in un thread separato
// 4. Gestisce la lista dei giocatori connessi
// ============================================================

#include "network.hpp"
#include <Geode/Geode.hpp>
#include <cstring>

using namespace geode::prelude;

// Singleton: restituisce sempre la stessa istanza
NetworkManager& NetworkManager::get() {
    static NetworkManager instance;
    return instance;
}

NetworkManager::~NetworkManager() {
    disconnect();
}

bool NetworkManager::connect(const std::string& ip, int port) {
    // Se siamo già connessi, prima disconnetti
    if (m_connected) disconnect();

    // Su Windows bisogna inizializzare Winsock
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log::error("Errore inizializzazione Winsock");
        return false;
    }
    #endif

    // Crea il socket UDP
    // AF_INET = IPv4, SOCK_DGRAM = UDP
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket == INVALID_SOCK) {
        log::error("Errore creazione socket");
        return false;
    }

    // Imposta timeout di ricezione (2 secondi)
    // Così il thread di ricezione non si blocca per sempre
    #ifdef _WIN32
    DWORD timeout = 2000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    #else
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    #endif

    // Configura l'indirizzo del server
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(port);  // htons converte in "network byte order"
    inet_pton(AF_INET, ip.c_str(), &m_serverAddr.sin_addr);

    m_connected = true;

    // Avvia il thread che ascolta i pacchetti in arrivo
    m_recvThread = std::thread(&NetworkManager::receiveLoop, this);

    log::info("Connesso al server {}:{}", ip, port);
    return true;
}

void NetworkManager::disconnect() {
    if (!m_connected) return;

    // Invia pacchetto LEAVE al server
    sendLeave();

    m_connected = false;

    // Aspetta che il thread di ricezione finisca
    if (m_recvThread.joinable()) {
        m_recvThread.join();
    }

    // Chiudi il socket
    #ifdef _WIN32
    closesocket(m_socket);
    WSACleanup();
    #else
    close(m_socket);
    #endif

    m_socket = INVALID_SOCK;

    // Pulisci la lista giocatori
    std::lock_guard<std::mutex> lock(m_playersMutex);
    m_otherPlayers.clear();

    log::info("Disconnesso dal server");
}

bool NetworkManager::isConnected() const {
    return m_connected;
}

// Invia un aggiornamento della posizione al server
void NetworkManager::sendPlayerUpdate(const PlayerData& data) {
    if (!m_connected) return;

    Packet pkt;
    pkt.type = PacketType::UPDATE;
    pkt.data = data;

    // sendto invia il pacchetto UDP al server
    sendto(m_socket, (char*)&pkt, sizeof(pkt), 0,
           (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));
}

// Invia un pacchetto "mi sono connesso"
void NetworkManager::sendJoin(const PlayerData& data) {
    if (!m_connected) return;

    Packet pkt;
    pkt.type = PacketType::JOIN;
    pkt.data = data;

    sendto(m_socket, (char*)&pkt, sizeof(pkt), 0,
           (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));

    log::info("Pacchetto JOIN inviato");
}

// Invia un pacchetto "me ne vado"
void NetworkManager::sendLeave() {
    if (!m_connected) return;

    Packet pkt;
    pkt.type = PacketType::LEAVE;
    pkt.data.playerId = 0;  // Il server identifica dal mittente

    sendto(m_socket, (char*)&pkt, sizeof(pkt), 0,
           (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));
}

void NetworkManager::setOnPlayerUpdate(std::function<void(const PlayerData&)> callback) {
    m_onUpdate = callback;
}

void NetworkManager::setOnPlayerLeave(std::function<void(uint32_t)> callback) {
    m_onLeave = callback;
}

std::vector<PlayerData> NetworkManager::getOtherPlayers() {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return m_otherPlayers;
}

// Thread di ricezione — gira in background e ascolta pacchetti
void NetworkManager::receiveLoop() {
    char buffer[sizeof(Packet)];

    while (m_connected) {
        // recvfrom aspetta un pacchetto (con timeout di 2 sec)
        int received = recvfrom(m_socket, buffer, sizeof(buffer), 0, nullptr, nullptr);

        if (received <= 0) continue;  // Timeout o errore, riprova
        if (received != sizeof(Packet)) continue;  // Pacchetto malformato

        Packet* pkt = reinterpret_cast<Packet*>(buffer);

        switch (pkt->type) {
            case PacketType::UPDATE:
            case PacketType::JOIN: {
                // Aggiorna la lista dei giocatori
                std::lock_guard<std::mutex> lock(m_playersMutex);

                bool found = false;
                for (auto& p : m_otherPlayers) {
                    if (p.playerId == pkt->data.playerId) {
                        p = pkt->data;  // Aggiorna dati esistenti
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m_otherPlayers.push_back(pkt->data);
                    log::info("Nuovo giocatore: {}", pkt->data.name);
                }

                // Chiama il callback se impostato
                if (m_onUpdate) m_onUpdate(pkt->data);
                break;
            }

            case PacketType::LEAVE: {
                std::lock_guard<std::mutex> lock(m_playersMutex);
                uint32_t id = pkt->data.playerId;

                // Rimuovi il giocatore dalla lista
                m_otherPlayers.erase(
                    std::remove_if(m_otherPlayers.begin(), m_otherPlayers.end(),
                        [id](const PlayerData& p) { return p.playerId == id; }),
                    m_otherPlayers.end()
                );

                if (m_onLeave) m_onLeave(id);
                log::info("Giocatore {} disconnesso", id);
                break;
            }

            case PacketType::PONG:
                // Il server ha risposto al nostro ping
                break;

            default:
                break;
        }
    }
}
