# Documentation Firmware AquaLook

Ce dossier décrit **comment le code existant fonctionne**. Chaque fiche est reliée aux fichiers sources, aux tomes Engineering et aux validations applicables.

## Lot F1

| Référence | Composant |
|---|---|
| `FW-001_main.md` | composition, boot et boucle principale |
| `FW-002_Scheduler.md` | planification et déclenchement |
| `FW-003_ConfigManager.md` | configuration et persistance |
| `FW-004_Runtime.md` | Runtime, profiler et diagnostics |
| `FW-005_EventLog.md` | journal d’événements et limites |

## Lot F2

| Référence | Composant |
|---|---|
| `FW-006_WebManager.md` | serveur HTTP, routes et actions différées |
| `FW-007_DisplayManager.md` | écran, tactile, vues et rafraîchissements |
| `FW-008_StorageManager.md` | microSD, ressources statiques et fallbacks |
| `FW-009_WiFiManager.md` | Wi-Fi STA/AP, reconnexion et portail captif |
| `FW-010_EquipmentManager.md` | modèle V4, plans et backends |

## Règle de maintenance

Toute modification majeure d’un composant met à jour sa fiche Firmware, le tome Engineering associé, le guide Developer concerné et le checkpoint.
