# 02 — Décisions d’architecture

## D001 — PlatformIO comme environnement de référence

`platformio.ini` est la source des dépendances, broches TFT et environnements. Toute dépendance doit être versionnée dans `lib_deps`.

## D002 — Architecture par managers

`main.cpp` assemble les modules. Les responsabilités métier restent dans les managers appropriés.

## D003 — Persistance NVS

La configuration active est stockée en NVS. Toute évolution de structure exige schéma, compatibilité et stratégie de migration.

## D004 — LittleFS en lecture pour les ressources

LittleFS sert le Web et le splash. ConfigManager est l’unique propriétaire du montage. Aucun fichier de travail dans `data/`.

## D005 — Activation relais par callback

ScheduleManager reste indépendant du matériel. Le callback est câblé dans `main.cpp`.

## D006 — EventBus minimal

Les communications transversales utilisent des flags statiques. Ne pas créer de second bus global.

## D007 — Interface Web statique embarquée

Chaque octet compte. Les gros frameworks frontend sont exclus et `buildfs` est obligatoire après changement.

## D008 — Limite fonctionnelle à 8 zones

La capacité interne reste à 16 pour compatibilité, mais l’interface et les sorties actives sont limitées à 8.

## D009 — Deux modes de planning

Modes supportés : jours fixes et intervalle. Toute nouvelle répétition exige une décision d’architecture.

## D010 — Verrouillage administrateur visuel

Le verrouillage actuel est côté navigateur. Il ne protège pas les secrets et ne constitue pas une authentification forte.

## D011 — Configuration LCD hot-reload

Les paramètres d’affichage prennent effet sans reboot via `EventBus::displayDirty`.

## D012 — Sécurité avant ergonomie

La logique relais, la durée maximale et la réponse HTTP avant reboot sont prioritaires sur les simplifications visuelles.

## D013 — Traçabilité du firmware par version Git

Chaque firmware AquaLook doit embarquer une identité de build issue de Git ou générée au moment de la compilation. Une source compile-time unique fournit au minimum le SHA Git court et la date/heure de compilation ; elle expose également la branche ou l’origine de build lorsqu’elle peut être déterminée de manière fiable.

Cette identité est réutilisée sans duplication manuelle par :

- la page Système du LCD ;
- la vue Web « À propos » ;
- les diagnostics système et les journaux de démarrage ;
- les futurs exports de diagnostic.

La vue Web « À propos » doit être facilement accessible depuis l’interface principale. Elle fait partie des ressources complètes servies prioritairement depuis la SD, mais une information de version minimale doit rester consultable lorsque l’interface de secours LittleFS est utilisée.
