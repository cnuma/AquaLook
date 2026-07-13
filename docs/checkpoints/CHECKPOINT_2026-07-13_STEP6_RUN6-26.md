# Checkpoint AquaLook — Étape 6 / RUN6.26

Date : 13 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche de clôture : `work/run6-26-stage6-closeout`
- Branche précédente validée : `work/run6-25-runtime-profiler`
- Dernier état matériel validé avant clôture : RUN6.25

## État matériel validé

- compilation environnement legacy : OK ;
- compilation environnement V4 et upload : OK ;
- démarrage matériel : OK ;
- interface écran : OK ;
- tactile XPT2046 : OK ;
- météo OpenWeatherMap : OK ;
- synchronisation NTP : OK ;
- serveur Web : OK ;
- pilotage relais : état validé conservé ;
- diagnostics runtime : OK ;
- aucun défaut matériel nouveau signalé.

## Chaîne de compilation et de test de référence

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

La commande séparée `pio run -e ProgrammeArrosage_v4` est volontairement supprimée : la cible `upload` compile déjà l’environnement V4 avant le téléversement.

## Travaux validés pendant la fin de l’étape 6

### RUN6.22 — runtime non bloquant

- lecture météo OWM en flux sans charger la réponse complète dans un `String` ;
- météo restaurée sur matériel ;
- suppression du redraw complet forcé lors de la première synchronisation NTP ;
- rafraîchissement dynamique de l’heure conservé ;
- timestamps relatifs ajoutés aux logs centralisés ;
- logs NTP migrés vers `EventLog` ;
- messages du profiler raccourcis pour rester lisibles.

### RUN6.23 — initialisation tactile SPI

- portée du masquage temporaire des logs ESP étendue à toute l’initialisation du tactile ;
- bus tactile VSPI séparé conservé ;
- fonctionnement tactile validé.

### RUN6.24 — ressource Web logo

- aucune modification nécessaire ;
- le code courant sert déjà `/www/logo.png` depuis la SD ;
- fallback LittleFS conservé ;
- fallback SVG embarqué disponible quand le logo manque sur SD et LittleFS.

### RUN6.25 — profiler runtime

- mesures identifiées comme temps mural et non temps CPU strict ;
- pauses supérieures à 100 ms qualifiées comme potentiellement liées à l’ordonnanceur ;
- `yield()` exclu des alertes de lenteur métier ;
- diagnostics JSON enrichis ;
- compilation et tests matériels validés.

## Invariants confirmés

1. L’écran tactile XPT2046 reste sur son bus VSPI séparé.
2. Les réponses réseau volumineuses ne doivent pas être chargées intégralement en mémoire sur une carte sans PSRAM lorsqu’un parsing en flux est possible.
3. `EventLog` reste le point central pour les événements de diagnostic applicatifs.
4. Les timestamps série sont relatifs au démarrage et servent à corréler les événements.
5. Les mesures du profiler basées sur `micros()` représentent du temps mural et peuvent inclure des préemptions FreeRTOS.
6. Un changement d’heure NTP ne doit pas provoquer un redraw complet de l’écran.
7. La chaîne de validation standard comprend une compilation legacy puis compilation+upload V4.

## Points volontairement non modifiés

- logique métier du planning ;
- modèle de zones ;
- topologie des relais ;
- persistance NVS ;
- comportement météo fonctionnel ;
- interface Web hors gestion des ressources statiques existantes ;
- exécution physique de la pompe, encore maintenue hors périmètre de cette clôture.

## Risques et limites connus

- le profiler ne mesure pas le temps CPU consommé par tâche ;
- une alerte de temps mural peut encore refléter une préemption système plutôt qu’un blocage du composant ;
- l’heure affichée peut être mise à jour avec le délai du refresh dynamique nominal ;
- les anciens logs utilisant directement `Serial.print()` ne sont pas tous horodatés ;
- la validation du logo dépend de la présence éventuelle de `/www/logo.png` sur la carte SD, mais le fallback firmware évite le 404.

## Statut de clôture

L’étape 6 est considérée comme fonctionnellement stabilisée sur matériel. Toute évolution suivante doit partir de `work/run6-26-stage6-closeout` ou d’un commit explicitement dérivé de cette branche.
