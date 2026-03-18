#pragma once

// ============================================================
// player_node.hpp — Nodo che disegna gli altri giocatori
// ============================================================
// Ogni giocatore remoto viene rappresentato da un 
// "RemotePlayer" che è un nodo Cocos2D (il motore grafico
// usato da Geometry Dash). Questo nodo viene aggiunto alla
// scena di gioco e si muove in base ai dati ricevuti dal server.
// ============================================================

#include <Geode/Geode.hpp>
#include "network.hpp"

using namespace geode::prelude;

class RemotePlayer : public CCNode {
public:
    // Crea un nuovo giocatore remoto con un ID
    static RemotePlayer* create(uint32_t playerId, const std::string& name);
    bool init(uint32_t playerId, const std::string& name);

    // Aggiorna posizione e stato del giocatore
    void updateFromData(const PlayerData& data);

    uint32_t getPlayerId() const { return m_playerId; }

private:
    uint32_t m_playerId;
    CCSprite* m_icon = nullptr;        // L'icona del cubo/nave
    CCLabelBMFont* m_nameLabel = nullptr;  // Il nome sopra l'icona

    // Per interpolazione (movimento fluido)
    float m_targetX = 0.f;
    float m_targetY = 0.f;
    float m_targetRotation = 0.f;

    void update(float dt) override;
};
