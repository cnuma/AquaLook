# 05 — Compilation et tests

## Environnements PlatformIO

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t buildfs
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
pio run -e calibration
pio run -e test_relais
```

## Précontrôles Git

```powershell
git status
git diff --check
git diff --stat
git diff
```

## Matrice de validation minimale

| Type de changement | Firmware | buildfs | Test Web | Test LCD | Test matériel |
|---|---:|---:|---:|---:|---:|
| C++ métier | Oui | Si data touché | Selon impact | Selon impact | Selon impact |
| HTML/JS/CSS | Oui | Oui | Oui | Si config partagée | Non |
| Config NVS | Oui | Oui | Oui | Oui | Souvent |
| Relais | Oui | Non sauf UI | Selon API | Selon UI | Obligatoire |
| TFT/touch | Oui | Non sauf assets | Non | Obligatoire | Obligatoire |
| Documentation | Non | Non | Non | Non | Non |

## Tests Web de non-régression

1. page principale ;
2. zones ;
3. planning ;
4. mode dense ;
5. modales ;
6. démarrage manuel ;
7. arrêt manuel ;
8. modification zone ;
9. sauvegarde créneaux ;
10. météo ;
11. info-bulles ;
12. paramètres utilisateur ;
13. verrouillage admin ;
14. mauvais mot de passe ;
15. bon mot de passe ;
16. persistance de session ;
17. reverrouillage ;
18. sauvegarde Wi-Fi avant reboot ;
19. logs ;
20. portail captif.

## Tests NVS

Boot sans NVS, sauvegarde, reboot, chargement, CRC invalide, taille invalide, reset, migration JSON et suppression JSON seulement après NVS valide.

## Tests planning

Heure non synchronisée, créneau actif/inactif, plusieurs zones, plusieurs créneaux, changement de jour, passage minuit, intervalle, pluie sous/au-dessus du seuil et durée maximale.

## Tests relais

Boot sûr, direct, inverse, XL9535, MCP23017 lorsque disponible, zone 1, dernière zone, arrêt manuel, timeout et reboot.

## Critères de livraison

Une livraison n’est pas valide sans compilation `SUCCESS`, buildfs `SUCCESS` si `data/` change, diff contrôlé, état Git explicite et liste des tests matériels non exécutés.
