// ============================================================
// main.cpp — File principale della mod Mini Globed
// ============================================================
// Qui usiamo il sistema di "hook" di Geode per intercettare
// le funzioni di Geometry Dash. In pratica diciamo:
// "Quando GD chiama la funzione X, esegui anche il mio codice"
//
// I punti principali dove ci agganciamo:
// 1. PlayLayer::init — Quando inizia un livello
// 2. PlayLayer::update — Ogni frame durante il gioco
// 3. PlayLayer::onQuit — Quando si esce dal livello
// ============================================================

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "network.hpp"
#include "player_node.hpp"

using namespace geode::prelude;

// ============================================================
// Variabili globali della mod
// ============================================================
static std::unordered_map<uint32_t, RemotePlayer*> g_remotePlayers;
static CCNode* g_playersContainer = nullptr;
static float g_sendTimer = 0.f;
static const float SEND_INTERVAL = 0.033f;  // ~30 pacchetti al secondo

// ID unico per questo giocatore (basato sul timestamp)
static uint32_t getMyPlayerId() {
    static uint32_t id = static_cast<uint32_t>(time(nullptr)) & 0xFFFFFF;
    return id;
}

// Legge le impostazioni dalla configurazione della mod
static std::string getServerIp() {
    return Mod::get()->getSettingValue<std::string>("server-ip");
}
static int getServerPort() {
    return Mod::get()->getSettingValue<int64_t>("server-port");
}
static std::string getPlayerName() {
    return Mod::get()->getSettingValue<std::string>("player-name");
}

// ============================================================
// Helper: crea i dati del giocatore locale
// ============================================================
PlayerData makeLocalPlayerData(PlayerObject* player, PlayLayer* playLayer) {
    PlayerData data{};
    data.playerId = getMyPlayerId();

    std::string name = getPlayerName();
    strncpy(data.name, name.c_str(), sizeof(data.name) - 1);

    if (player) {
        data.posX = player->getPositionX();
        data.posY = player->getPositionY();
        data.rotation = player->getRotation();
        data.isDead = player->m_isDead;
    }

    if (playLayer && playLayer->m_level) {
        data.levelId = playLayer->m_level->m_levelID;
    }

    data.iconScale = 1.0f;
    return data;
}

// ============================================================
// Hook su PlayLayer — Quando inizia un livello
// ============================================================
class $modify(MiniGlobedPlayLayer, PlayLayer) {

    // Chiamato quando il livello viene creato
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // Prima esegui la funzione originale di GD
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        log::info("Mini Globed: livello avviato!");

        // Crea un contenitore per gli sprite dei giocatori remoti
        g_playersContainer = CCNode::create();
        g_playersContainer->setZOrder(100);  // Sopra gli oggetti del livello

        // Aggiungi il contenitore alla "object layer" del livello
        // Così si muove con la camera del gioco
        if (this->m_objectLayer) {
            this->m_objectLayer->addChild(g_playersContainer);
        }

        // Pulisci la mappa dei giocatori remoti
        g_remotePlayers.clear();
        g_sendTimer = 0.f;

        // Connetti al server
        auto& net = NetworkManager::get();
        if (!net.isConnected()) {
            net.connect(getServerIp(), getServerPort());
        }

        // Invia pacchetto JOIN
        if (net.isConnected()) {
            PlayerData joinData = makeLocalPlayerData(this->m_player1, this);
            net.sendJoin(joinData);
        }

        return true;
    }

    // Chiamato ogni frame (~60fps)
    void update(float dt) {
        // Prima esegui l'update originale di GD
        PlayLayer::update(dt);

        auto& net = NetworkManager::get();
        if (!net.isConnected()) return;

        // ---- INVIO: manda la nostra posizione al server ----
        g_sendTimer += dt;
        if (g_sendTimer >= SEND_INTERVAL) {
            g_sendTimer = 0.f;

            if (this->m_player1) {
                PlayerData data = makeLocalPlayerData(this->m_player1, this);
                net.sendPlayerUpdate(data);
            }
        }

        // ---- RICEZIONE: aggiorna le posizioni degli altri ----
        auto others = net.getOtherPlayers();
        for (const auto& pdata : others) {
            // Ignora noi stessi
            if (pdata.playerId == getMyPlayerId()) continue;

            // Ignora giocatori in altri livelli
            if (this->m_level && pdata.levelId != (uint32_t)this->m_level->m_levelID) continue;

            // Cerca se abbiamo già uno sprite per questo giocatore
            auto it = g_remotePlayers.find(pdata.playerId);
            if (it != g_remotePlayers.end()) {
                // Aggiorna la posizione
                it->second->updateFromData(pdata);
            } else {
                // Nuovo giocatore! Crea uno sprite
                auto remotePlayer = RemotePlayer::create(pdata.playerId, pdata.name);
                if (remotePlayer && g_playersContainer) {
                    g_playersContainer->addChild(remotePlayer);
                    g_remotePlayers[pdata.playerId] = remotePlayer;
                    remotePlayer->updateFromData(pdata);
                    log::info("Aggiunto sprite per giocatore: {}", pdata.name);
                }
            }
        }

        // Rimuovi giocatori che non sono più nella lista
        std::vector<uint32_t> toRemove;
        for (auto& [id, node] : g_remotePlayers) {
            bool found = false;
            for (const auto& p : others) {
                if (p.playerId == id) { found = true; break; }
            }
            if (!found) toRemove.push_back(id);
        }
        for (uint32_t id : toRemove) {
            if (g_remotePlayers[id]) {
                g_remotePlayers[id]->removeFromParent();
            }
            g_remotePlayers.erase(id);
        }
    }

    // Chiamato quando si esce dal livello
    void onQuit() {
        log::info("Mini Globed: uscita dal livello");

        // Disconnetti dal server
        NetworkManager::get().disconnect();

        // Pulisci tutto
        g_remotePlayers.clear();
        g_playersContainer = nullptr;

        // Chiama la funzione originale
        PlayLayer::onQuit();
    }
};

// ============================================================
// Hook sul Menu — Mostra stato connessione
// ============================================================
class $modify(MiniGlobedMenuLayer, MenuLayer) {

    bool init() {
        if (!MenuLayer::init()) return false;

        // Aggiungi un'etichetta di stato nel menu principale
        auto statusLabel = CCLabelBMFont::create("Mini Globed v1.0", "bigFont.fnt");
        statusLabel->setScale(0.3f);
        statusLabel->setOpacity(150);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        statusLabel->setPosition({winSize.width - 80, 15});

        this->addChild(statusLabel, 100);

        return true;
    }
};
