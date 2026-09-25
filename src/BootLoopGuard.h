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
//  ── Sortie du mode dégradé — à L'ESSAI, pas en aveugle ──────────────────
//
//  Ajouté le 25 septembre 2026. Rester dégradé pour toujours a son propre
//  coût : plus personne ne regarde un voyant ambre qui dure depuis des
//  jours, et le module tourne alors durablement sans météo ni mise à jour
//  sans que qui que ce soit s'en avise. Mais l'avertissement d'origine
//  reste vrai mot pour mot : « survivre une heure avec la météo coupée ne
//  prouve rien sur la météo ». Rester passif ne le contredit pas — le
//  résoudre, si.
//
//  Le mécanisme, en deux temps, sur le MÊME principe que le compteur de
//  démarrages suspects : ne jamais se croire stable sans avoir mis le
//  suspect à l'épreuve.
//
//   1. Après STABLE_UPTIME_MS en mode dégradé sans incident, les fonctions
//      suspendues sont RÉELLEMENT relancées (isDegraded() passe à faux —
//      WeatherManager, UpdateCheckScheduler et NotificationManager le
//      vérifient en direct, pas seulement au démarrage) — mais le module
//      reste SOUS SURVEILLANCE : un marqueur « à l'essai » est posé en NVS.
//   2. Si un redémarrage NON PLANIFIÉ survient avant que ce même démarrage
//      n'ait à son tour tenu STABLE_UPTIME_MS de plus, c'est la preuve
//      qu'une fonction relancée est bien en cause : re-dégradation
//      IMMÉDIATE, sans repasser par les quatre coups du compteur normal —
//      la preuve directe vaut mieux que l'heuristique. Aucun nouvel essai
//      automatique n'est retenté pour cet épisode : il faut alors un geste
//      explicite (clearDegraded()), comme avant. Un redémarrage VOULU
//      pendant l'essai (mise à jour, etc.) n'est ni une preuve ni une
//      réfutation : l'essai s'arrête sans jugement, la surveillance
//      normale reprend sur le démarrage suivant.
//      Si aucun redémarrage non planifié ne survient, l'essai est
//      confirmé : le module envoie UNE notification — désormais sûre,
//      puisque les notifications viennent justement d'être revalidées —
//      pour que l'utilisateur sache que l'épisode a eu lieu et se pose la
//      question de sa cause (alimentation, réseau, carte SD...).
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

    // Sortie du mode dégradé, sur action explicite de l'utilisateur. Efface
    // aussi le drapeau « essai déjà tenté et raté » : un geste manuel rouvre
    // le droit à un futur essai automatique.
    static bool clearDegraded();

    // Vrai entre le moment où les fonctions suspendues sont relancées à
    // l'essai et leur confirmation (ou leur échec). Les indicateurs visuels
    // (bandeau LCD, pastille Web) s'en servent pour distinguer « dégradé »,
    // « à l'essai » et « normal ».
    static bool isOnProbation();

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
    // Relance reellement les fonctions suspendues (isDegraded() -> false) et
    // pose la surveillance NVS. `triggeringCount` est repris tel quel dans
    // la notification eventuelle : le nombre de demarrages sans stabilite
    // qui avaient declenche l'episode.
    static void beginProbation(uint8_t triggeringCount);
    // L'essai a tenu STABLE_UPTIME_MS de plus sans incident : confirme,
    // notifie, referme l'episode.
    static void confirmHealed();
    // Un redemarrage NON PLANIFIE est survenu pendant l'essai : preuve
    // directe, re-degradation immediate, plus de nouvel essai automatique
    // pour cet episode.
    static void relapse();

    static uint8_t _suspectCount;
    static bool _degraded;
    static bool _cleared;
    static bool _started;

    // Essai en cours (fonctions relancees, sous surveillance) et instant de
    // depart, pour mesurer la seconde fenetre de stabilite.
    static bool _onProbation;
    static uint32_t _probationStartMs;
};
