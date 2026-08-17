#pragma once

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  BootLoopGuard — dernier rempart contre une boucle de redémarrages
//
//  Le 17 août 2026, le module est parti deux fois en boucle de redémarrages :
//  une notification de mise à jour qui faisait déborder une pile, puis une
//  réponse météo de 17 Ko qui épuisait le tas et faisait échouer l'allocation
//  de la connexion HTTP suivante — `operator new` lève, personne ne rattrape,
//  et le runtime C++ abat tout le système.
//
//  Les deux causes ont été corrigées. Ce n'est pas le sujet ici. Le sujet est
//  qu'aucun des deux n'a été arrêté par le module lui-même : le seul rempart a
//  été un utilisateur qui a débranché la prise. Pour un appareil censé
//  fonctionner seul chez quelqu'un qui n'est pas informaticien, c'est le
//  défaut le plus grave de la journée — bien plus que les bogues qui l'ont
//  révélé, car il vaut pour toutes les causes futures, y compris inconnues.
//
//  Principe : compter les démarrages qui ne sont suivis d'aucune période de
//  fonctionnement stable. Au-delà d'un seuil, démarrer en MODE DÉGRADÉ, où
//  seules subsistent les fonctions essentielles.
//
//  ── Ce qui survit et ce qui est sacrifié ────────────────────────────────
//
//  Survivent : l'arrosage, l'horloge, les relais, l'écran et le serveur Web.
//  L'arrosage parce que c'est la raison d'être de l'appareil ; le serveur Web
//  et l'écran parce que sans eux l'utilisateur n'aurait aucun moyen de
//  comprendre ni de reprendre la main.
//
//  Sont suspendus : la météo, la vérification de mise à jour et les
//  notifications réseau. Ce sont des fonctions de confort, et ce sont
//  précisément celles qui ont provoqué les deux boucles observées.
//
//  Les notifications sont volontairement suspendues elles aussi, alors même
//  qu'elles seraient le moyen le plus commode de prévenir l'utilisateur : si
//  la boucle vient de l'envoi d'une notification — cas réellement survenu —
//  en émettre une pour signaler le mode dégradé relancerait exactement ce que
//  ce mode existe pour arrêter. L'état est donc signalé par les moyens locaux,
//  qui ne peuvent rien relancer : voyant de défaut, écran, page Web, journal.
//
//  ── Sortie du mode dégradé ──────────────────────────────────────────────
//
//  Le mode dégradé ne s'éteint jamais tout seul. Survivre une heure avec la
//  météo coupée ne prouve rien sur la météo : un retour automatique
//  relancerait la boucle au premier cycle suivant. Il faut donc une action
//  explicite de l'utilisateur, une fois qu'il a vu ce qui se passait.
//
//  ── Pas de faux positif ─────────────────────────────────────────────────
//
//  Un redémarrage voulu — mise à jour, mode maintenance, changement de WiFi —
//  ne doit pas être compté. Il passe par restartDeliberately(), qui pose une
//  marque en NVS avant de redémarrer. L'absence de marque vaut « non voulu » :
//  un plantage ne peut pas, par construction, marquer son propre redémarrage.
//  C'est le sens sûr de l'oubli, et il vaut aussi pour le code futur.
// ═══════════════════════════════════════════════════════════════

class BootLoopGuard {
public:
    // À appeler très tôt dans setup(), avant toute initialisation lourde et
    // avant les sous-systèmes susceptibles d'être suspendus.
    static void onBoot();

    // À appeler dans loop(). Efface le compteur une fois la durée de stabilité
    // atteinte ; ne fait rien ensuite.
    static void update();

    // Vrai si ce démarrage s'est fait en mode dégradé. Les sous-systèmes de
    // confort doivent s'abstenir tant que c'est le cas.
    static bool isDegraded();

    // Nombre de démarrages consécutifs sans période stable, tel que lu au
    // démarrage. Exposé pour le diagnostic.
    static uint8_t suspectBootCount();

    // Sortie du mode dégradé, sur action explicite de l'utilisateur.
    static bool clearDegraded();

    // Seul chemin autorisé pour un redémarrage voulu. Marque le redémarrage
    // comme attendu, journalise sa raison, puis redémarre.
    static void restartDeliberately(const char* reason);

    // Quatre démarrages sans stabilité : au-delà du hasard, en deçà d'une
    // réaction précipitée. Une coupure de courant suivie d'une reprise
    // difficile ne doit pas suffire à dégrader l'appareil.
    static constexpr uint8_t DEGRADED_THRESHOLD = 4U;

    // Durée de fonctionnement au-delà de laquelle un démarrage est considéré
    // comme réussi. Doit rester nettement supérieure à la période des boucles
    // observées (26 s puis 50 s le 17 août 2026) : trop courte, une boucle
    // lente serait comptée comme une suite de démarrages sains.
    static constexpr uint32_t STABLE_UPTIME_MS = 180000UL;  // 3 min

private:
    static void persistCount(uint8_t count);

    static uint8_t _suspectCount;
    static bool _degraded;
    static bool _cleared;
    static bool _started;
};
