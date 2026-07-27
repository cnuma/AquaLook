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

## Lot F3

| Référence | Composant |
|---|---|
| `FW-011_RelaisManager_et_RelayTopology.md` | topologie, mappings et commandes I²C des relais |
| `FW-012_WeatherManager.md` | récupération et agrégation météo non bloquantes |
| `FW-013_NTPManager.md` | synchronisation et accès à l’heure système |
| `FW-014_FaultManager.md` | défauts actifs, erreurs et acquittement |

## Lot F4

| Référence | Composant |
|---|---|
| `FW-015_V4PilotRuntime.md` | assemblage du pilote et du backend physique V4 |
| `FW-016_EquipmentExecutionEngine_et_ShadowRuntime.md` | exécution passive par zone et comparaison shadow |
| `FW-017_EquipmentRuntimeConfigStore.md` | persistance et valeurs sûres de la configuration V4 |
| `FW-018_Adaptateurs_backends_et_etat_partage_XL9535.md` | coexistence legacy/V4 et image commune des sorties |

## Règle de maintenance

Toute modification majeure d’un composant met à jour sa fiche Firmware, le tome Engineering associé, le guide Developer concerné et le checkpoint.
