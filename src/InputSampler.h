#pragma once

#include <stdint.h>

#include "RelayTopology.h"

// Echantillonnage et ANTI-REBOND des entrees tout ou rien.
//
// POURQUOI UN SCRIPT NE DOIT JAMAIS LIRE LA BROCHE DIRECTEMENT
//
// Un flotteur de cuve claquette : la surface de l'eau oscille, le contact
// s'ouvre et se ferme des dizaines de fois en quelques secondes. Un script
// branche sur la broche brute ouvrirait et fermerait une vanne en rafale --
// et userait le relais bien avant d'avoir arrose quoi que ce soit.
//
// Le script lit donc une valeur STABILISEE, jamais la broche.
//
// LA REGLE DE STABILITE
//
// Une valeur ne devient la valeur officielle qu'apres etre restee identique
// pendant STABLE_SAMPLES lectures consecutives. A un echantillon toutes les
// SAMPLE_MS, cela demande une seconde de calme.
//
// Une seconde est un compromis assume : assez long pour ignorer le clapot
// d'une cuve, assez court pour qu'une coupure d'eau reelle soit vue avant
// que l'arrosage n'ait fait de degat. Un reglage par entree viendra si le
// terrain montre que ce choix unique ne suffit pas.
//
// CE QU'ON NE FAIT PAS
//
// Aucune lecture I2C n'est faite a la demande du script : elle prendrait le
// bus au milieu d'une commande de vanne, et sa duree dependrait du script.
// L'echantillonnage est periodique et borne ; le script, lui, lit de la
// memoire.

class InputSampler {
public:
    // Un echantillon toutes les 100 ms : dix fois moins que la periode de
    // rebond typique d'un contact mecanique, et assez espace pour ne pas
    // encombrer un bus partage avec les vannes.
    static constexpr uint32_t SAMPLE_MS = 100U;
    static constexpr uint8_t  STABLE_SAMPLES = 10U;
    static constexpr uint8_t  MAX_INPUTS = RelayTopology::RESERVED_INPUT_ASSIGNMENTS;

    using Reader = bool (*)(uint16_t inputId, bool& active);

    // verbose=false pour les autotests : un echantillonneur jouet ne doit
    // pas ecrire dans le journal du module. Une entree 600 qui n'existe pas,
    // vue par quelqu'un cherchant une panne, coute plus cher que le confort
    // de reutiliser la meme classe.
    void begin(const RelayTopology::RelayTopologyConfig* topology, Reader reader,
               bool verbose = true);
    void update(uint32_t nowMs);

    // Valeur stabilisee. false si l'entree est inconnue ou si aucune valeur
    // n'a encore tenu assez longtemps -- ne jamais confondre "pas encore
    // etabli" avec "inactif", d'ou le booleen de retour distinct.
    bool read(uint16_t inputId, bool& active) const;

    // Diagnostic : ce que dit la broche a l'instant, et combien de fois la
    // valeur stabilisee a change. Un compteur qui s'emballe designe un
    // cablage a revoir bien mieux qu'une valeur instantanee.
    bool readRaw(uint16_t inputId, bool& active) const;
    uint32_t transitions(uint16_t inputId) const;
    uint8_t count() const { return _count; }
    uint16_t idAt(uint8_t i) const { return i < _count ? _slots[i].id : 0U; }

private:
    struct Slot {
        uint16_t id;
        bool raw;
        bool stable;
        bool established;
        bool present;      // la carte a repondu a la derniere lecture
        uint8_t steady;    // echantillons consecutifs identiques
        uint32_t changes;
    };

    int8_t indexOf(uint16_t inputId) const;

    const RelayTopology::RelayTopologyConfig* _topology = nullptr;
    Reader _reader = nullptr;
    Slot _slots[MAX_INPUTS] = {};
    uint8_t _count = 0U;
    uint32_t _lastSampleMs = 0U;
    bool _verbose = true;
};
