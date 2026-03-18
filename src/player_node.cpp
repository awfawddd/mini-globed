// ============================================================
// player_node.cpp — Disegna gli altri giocatori nel livello
// ============================================================
// Usa il motore grafico Cocos2D (integrato in GD) per creare
// uno sprite (icona) che si muove in base ai dati di rete.
// L'interpolazione (lerp) rende il movimento fluido anche
// se i pacchetti arrivano 20-30 volte al secondo.
// ============================================================

#include "player_node.hpp"

RemotePlayer* RemotePlayer::create(uint32_t playerId, const std::string& name) {
    auto ret = new RemotePlayer();
    if (ret && ret->init(playerId, name)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool RemotePlayer::init(uint32_t playerId, const std::string& name) {
    if (!CCNode::init()) return false;

    m_playerId = playerId;

    // Crea l'icona del giocatore (usiamo un quadrato come placeholder)
    // In una versione avanzata qui caricheresti l'icona vera del giocatore
    m_icon = CCSprite::create("player_ball_01_001.png");
    if (!m_icon) {
        // Fallback: usa un quadrato colorato se la texture non esiste
        m_icon = CCSprite::create("GJ_square01.png");
    }
    if (m_icon) {
        m_icon->setScale(0.8f);
        // Colore azzurro per distinguere i giocatori remoti
        m_icon->setColor({100, 200, 255});
        m_icon->setOpacity(180);  // Leggermente trasparente
        this->addChild(m_icon, 1);
    }

    // Crea l'etichetta con il nome
    m_nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
    if (m_nameLabel) {
        m_nameLabel->setScale(0.35f);
        m_nameLabel->setPosition({0, 25});  // Sopra l'icona
        m_nameLabel->setOpacity(200);
        this->addChild(m_nameLabel, 2);
    }

    // Attiva il metodo update() che viene chiamato ogni frame
    this->scheduleUpdate();

    return true;
}

void RemotePlayer::updateFromData(const PlayerData& data) {
    // Salva la posizione target — l'interpolazione in update()
    // farà muovere lo sprite gradualmente verso questa posizione
    m_targetX = data.posX;
    m_targetY = data.posY;
    m_targetRotation = data.rotation;

    // Aggiorna visibilità in base allo stato
    if (data.isDead) {
        this->setVisible(false);
    } else {
        this->setVisible(true);
    }
}

// Chiamato ogni frame (~60 volte al secondo)
void RemotePlayer::update(float dt) {
    // Interpolazione lineare (lerp) per movimento fluido
    // 0.3f = fattore di smoothing (più basso = più fluido ma più ritardo)
    float lerpFactor = 0.3f;

    float currentX = this->getPositionX();
    float currentY = this->getPositionY();

    // lerp: posizione = posizione_attuale + (target - attuale) * fattore
    float newX = currentX + (m_targetX - currentX) * lerpFactor;
    float newY = currentY + (m_targetY - currentY) * lerpFactor;

    this->setPosition({newX, newY});

    // Interpola anche la rotazione
    if (m_icon) {
        float currentRot = m_icon->getRotation();
        float newRot = currentRot + (m_targetRotation - currentRot) * lerpFactor;
        m_icon->setRotation(newRot);
    }
}
