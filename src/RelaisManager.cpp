#include "RelaisManager.h"
#include "RelayTopologyStore.h"
#include "ConfigManager.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"

void RelaisManager::setXl9535SharedOutputState(
    AquaLook::Domain::Xl9535SharedOutputState* sharedOutputState
) {
    _xl9535SharedOutputState = sharedOutputState;
}

void RelaisManager::begin(ConfigManager* config) {
    _config = config;
    buildRuntimeTopology();

    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        _state[i] = false;
        _startMs[i] = 0;
    }

    for (uint8_t a = 0; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; a++) {
        _assignmentState[a] = false;
    }

    for (uint8_t b = 0; b < RelayTopology::MAX_RELAY_BOARDS; b++) {
        const RelayTopology::RelayBoardConfig& board = _topology.boards[b];
        const bool inv = (board.logic == RelayTopology::LOGIC_INVERTED);
        _regP0[b] = inv ? 0xFF : 0x00;
        _regP1[b] = inv ? 0xFF : 0x00;
        _boardReady[b] = false;

        if (_xl9535SharedOutputState &&
            RelayTopology::validateBoard(board) &&
            board.controller == RelayTopology::CONTROLLER_XL9535) {
            const uint16_t value = static_cast<uint16_t>(_regP0[b]) |
                static_cast<uint16_t>(static_cast<uint16_t>(_regP1[b]) << 8U);
            _xl9535SharedOutputState->seed(board.i2cAddress, value);
        }
    }

    // Le bus appartient a V4. RelaisManager ne touche plus une seule broche :
    // il ne tient plus que le cablage et l'etat des zones, que l'ecran, le Web
    // et le planificateur lisent. La sante du materiel est rapportee par le
    // pilote V4, qui est le seul a lui parler.
    _hardwareReady = true;

    // Trois etats a ne pas confondre, parce qu'ils appellent trois gestes
    // differents de la part de l'utilisateur :
    //   - non cable      -> il doit CONFIGURER (etat normal a la sortie de
    //                       l'usine, aucun defaut a signaler) ;
    //   - cable, HS      -> il doit VERIFIER LE MATERIEL (defaut I2C) ;
    //   - cable, OK      -> rien a faire.
    // Allumer le defaut I2C sur un module neuf l'enverrait chercher une
    // panne inexistante.
    const bool wired = RelayTopology::isWired(_topology);
    FaultManager::setActive(FaultId::RELAY_I2C, wired && !_hardwareReady);

    if (!wired) {
        EventLog::log(
            LOG_WARN,
            "Relais: aucun cablage enregistre, aucune sortie pilotable"
        );
    } else if (_hardwareReady) {
        // "init OK" laissait croire que cette classe avait initialise le
        // materiel. Dans le firmware V4 elle n'y touche plus : elle decrit le
        // cablage, c'est le pilote V4 qui l'initialise et le dit lui-meme.
        EventLog::log(
            LOG_INFO,
            "Relais: cablage retenu, %u voie(s) declaree(s)",
            RelayTopology::totalEnabledChannels(_topology)
        );
    } else {
        EventLog::log(
            LOG_ERROR,
            "Relais: aucune carte relais I2C initialisee"
        );
    }
}

// Le cablage est une DONNEE de configuration, jamais une deduction.
//
// Jusqu'au 7 septembre 2026 un module sans cablage enregistre s'en inventait
// un (une carte, les zones cablees dans l'ordre) a partir du controleur et de
// la logique saisis dans le menu Zones. C'etait commode et c'etait dangereux :
// l'interface elle-meme avertissait qu'un mauvais choix de logique "peut
// activer les relais au demarrage". Deviner le cablage, c'est risquer d'ouvrir
// la mauvaise vanne en silence.
//
// Desormais : pas de cablage enregistre = aucune sortie pilotee, et le module
// le dit -- sur l'ecran comme sur le Web. Il n'arrose pas tant qu'il n'est pas
// renseigne, ce qui est le comportement voulu : ne rien faire est toujours
// preferable a faire n'importe quoi sur un circuit d'eau.
void RelaisManager::buildRuntimeTopology() {
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;

    if (RelayTopologyStore::load(_topology, nbZ)) {
        _topologyFromStore = true;
        if (RelayTopology::hasDuplicateMappings(_topology, nbZ)) {
            EventLog::log(LOG_ERROR,
                          "Relais: cablage invalide, doublon de mapping");
        }
        const RelayTopology::RelayBoardConfig& p0 = _topology.boards[0];
        EventLog::log(LOG_INFO,
                      "Relais: cablage NVS, carte0=%s 0x%02X, voies=%u",
                      RelayTopology::controllerName(p0.controller),
                      p0.i2cAddress, p0.channelCount);
        return;
    }

    _topologyFromStore = false;
    RelayTopology::clear(_topology);
    EventLog::log(LOG_WARN,
                  "Relais: module non cable -- interface web, Zones > "
                  "Cablage relais");
}



void RelaisManager::update() {
    const uint32_t now = millis();
    const uint32_t maxMs = maxWateringMs();
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;

    // La coupure de securite ne commande PLUS elle-meme le relais.
    //
    // Elle appelait setRelay(), c'est-a-dire l'etage I2C historique -- y
    // compris dans le firmware V4, ou tout le reste passe par le nouveau
    // moteur. Une securite qui emprunte un autre chemin que le pilotage
    // normal est une securite qu'on ne teste jamais avec le code qu'on
    // utilise.
    //
    // Elle publie desormais l'intention ; l'appelant coupe par le meme
    // chemin que n'importe quelle autre commande.
    for (uint8_t i = 0; i < nbZ; i++) {
        if (_state[i] && _startMs[i] > 0 &&
            now - _startMs[i] >= maxMs) {
            EventLog::log(
                LOG_ERROR,
                "Relais: securite, zone %u a couper apres %lus",
                i + 1, maxMs / 1000UL
            );
            _safetyCutMask = static_cast<uint16_t>(_safetyCutMask | (1U << i));
        }
    }
}

int16_t RelaisManager::findZoneAssignment(uint8_t zone, uint8_t nbZones) const {
    if (zone >= nbZones || zone >= MAX_ZONES) return -1;

    for (uint8_t a = 0; a < RelayTopology::MAX_RELAY_ASSIGNMENTS; a++) {
        if (!RelayTopology::validateAssignment(_topology, a)) continue;
        const RelayTopology::RelayAssignment& assignment = _topology.assignments[a];
        if (assignment.role == RelayTopology::ROLE_ZONE_VALVE &&
            assignment.targetIndex == zone) {
            return a;
        }
    }

    return -1;
}

bool RelaisManager::zoneHasOutput(uint8_t zone) const {
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    return RelayTopology::resolveZoneValve(_topology, zone, nbZ).valid;
}



bool RelaisManager::getState(uint8_t relay) const {
    return relay < MAX_ZONES ? _state[relay] : false;
}

bool RelaisManager::getAssignmentState(uint8_t assignmentIndex) const {
    return assignmentIndex < RelayTopology::MAX_RELAY_ASSIGNMENTS
        ? _assignmentState[assignmentIndex]
        : false;
}

const RelayTopology::RelayTopologyConfig& RelaisManager::topology() const {
    return _topology;
}




uint8_t RelaisManager::nbRelaisPhysical() const {
    return _config ? _config->nbRelais() : NB_ZONES;
}

uint32_t RelaisManager::maxWateringMs() const {
    return _config
        ? static_cast<uint32_t>(_config->system().maxWateringMin) * 60000UL
        : MAX_WATERING_DURATION_MS;
}
