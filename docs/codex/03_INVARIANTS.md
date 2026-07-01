# 03 — Invariants

Les numéros ci-dessous reprennent les identifiants réellement présents dans les sources.

## Invariants explicites

| ID | Règle |
|---|---|
| I1 | `ConfigManager` est l’unique propriétaire de `LittleFS.begin()`. |
| I2 | LittleFS reste en lecture pour les ressources Web ; seul ConfigManager gère la persistance. |
| I3 | Un setter Web persistant met à jour la RAM et ConfigManager. |
| I4 | `fillScreen()` n’est appelé que dans le chemin de redraw complet contrôlé. |
| I5 | Le XPT2046 utilise son bus VSPI séparé. |
| I6 | L’activation relais du planificateur passe par callback, câblé dans `main.cpp`. |
| I7 | `RelaisManager::update()` maintient la sécurité de durée maximale. |
| I9 | Les identifiants Wi-Fi viennent de la configuration flash/NVS. |
| I10 | Lors d’une modification Wi-Fi avec reboot, la réponse HTTP est envoyée avant le redémarrage. |
| I11/I12 | Les structs de configuration utilisent des constructeurs explicites, pas des macros dans les initializers. |
| I12 | `config.h` ne contient que le compile-time. La duplication historique `src/` / `include/` reste à surveiller. |
| I15 | Le rendu utilise un sprite bouton unique successivement pour limiter la RAM. |
| I16 | Le cache HOME limite les redraws des boutons selon un seuil. |
| I18 | `EventBus` est le canal inter-modules transversal. |
| I19 | Les états portail captif et connecté sont exclusifs. |
| I20 | NTP et OWM relisent leurs paramètres après `configDirty`. |
| I21 | Le refresh LCD a un intervalle nominal et un intervalle actif configurables. |
| I26 | Une action manuelle depuis l’écran revient automatiquement à HOME. |
| I31 | Le hot-reload du thème détecte les changements sans reboot. |

## Invariants fonctionnels complémentaires

- **F1 — Non-blocage :** la boucle d’arrosage reste non bloquante et utilise l’état et `millis()`.
- **F2 — Heure requise :** un planning automatique ne s’exécute que lorsque NTP est synchronisé.
- **F3 — Pluie :** le blocage météo s’applique aux cycles planifiés selon le seuil et la fenêtre de la zone.
- **F4 — Limites :** 1 à 8 zones actives, capacité interne 16, 7 jours, 5 créneaux par jour et zone, fenêtre météo 48 h maximum.
- **F5 — IDs frontend :** les IDs de `index.html` consommés par `app.js` sont stables.
- **F6 — Routes :** les routes Web existantes sont des contrats.
- **F7 — LittleFS :** un buildfs réussi est obligatoire pour toute modification de `data/`.
- **F8 — NVS :** la configuration valide est contrôlée par magic, schéma, taille et CRC.
- **F9 — Migration :** l’ancien JSON n’est supprimé qu’après vérification du bloc NVS migré.
- **F10 — Contrôleur relais :** le nombre de zones dépend du contrôleur ; une mauvaise logique peut activer les relais au boot.
- **F11 — Noms de fichiers :** un fichier modifié, corrigé ou rendu à l’utilisateur conserve exactement son nom d’origine. Aucun suffixe, préfixe, renommage ou nom alternatif n’est autorisé sans demande explicite de l’utilisateur. Lorsqu’un fichier doit remplacer un fichier existant, il est fourni sous le nom exact du fichier cible.

## Modification d’un invariant

Toute modification doit être explicitement demandée, documentée dans `02_DECISIONS.md`, testée et accompagnée d’un checkpoint.
