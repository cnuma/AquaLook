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
- **F12 — Version Git sur LCD :** la page Système de l’écran LCD permet de consulter sans outil externe l’identifiant de version Git intégré au firmware. L’information affichée doit permettre d’identifier sans ambiguïté le build réellement exécuté, au minimum par le SHA Git court ; la branche ou l’origine de build et la date de compilation sont affichées si l’espace le permet.
- **F13 — À propos Web :** l’interface Web dispose d’un accès simple et visible à une page ou un panneau « À propos ». Cette vue affiche au minimum le SHA Git court du firmware, l’origine ou la branche de build lorsqu’elle est disponible, la date et l’heure de compilation, ainsi que la version applicative. Elle reste consultable lorsque l’interface complète est servie depuis la SD et doit conserver un mode de consultation minimal dans l’interface de secours.
- **F14 — Source unique de version :** les informations de version affichées sur le LCD, la page Web « À propos », les diagnostics et les journaux proviennent d’une même source compile-time générée ou centralisée. Ne jamais maintenir manuellement plusieurs valeurs de version susceptibles de diverger.
- **F15 — Marge mémoire et flux externes :** toute évolution qui ajoute une tâche, un buffer, un cache, un document JSON, un `String` volumineux ou une structure persistante doit réévaluer la mémoire transitoire maximale du chemin complet. Sur une cible sans PSRAM, une réponse réseau volumineuse ne doit pas être copiée intégralement avant traitement lorsqu’un parsing ou une consommation en flux est possible. Une fonction précédemment stable doit être retestée après toute hausse d’empreinte RAM globale, même si son propre code n’a pas changé.
- **F16 — Artefacts applicables vérifiés :** tout patch, diff, script, archive, fichier de configuration ou fichier de transformation fourni pour application doit être validé avant publication avec l’outil exact prévu et sur une base fidèle. Pour un patch Git, la validation minimale obligatoire comprend `git apply --check`, une application réelle dans une copie temporaire, puis `git diff --check`. Un artefact non vérifié ne doit jamais être présenté comme prêt à l’emploi.

## Modification d’un invariant

Toute modification doit être explicitement demandée, documentée dans `02_DECISIONS.md`, testée et accompagnée d’un checkpoint.
