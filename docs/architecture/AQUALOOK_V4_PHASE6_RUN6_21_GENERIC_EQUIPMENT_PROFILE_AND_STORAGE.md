# AquaLook V4 — Phase 6 — Run 6.21

## Décision d’architecture

La pompe n’est plus considérée comme une rubrique particulière de configuration. Elle devient un équipement géré par le même profil générique que les vannes, éclairages, ventilateurs, brumisateurs et contacts auxiliaires.

## Profil générique

Chaque équipement déclare :

- son activation ;
- son mode `disabled`, `shadow` ou `physical` ;
- son type métier ;
- son index cible ;
- son affectation relais ;
- ses délais de démarrage et d’arrêt ;
- ses durées minimales ON et OFF ;
- son nom.

Chaque zone déclare une vanne principale et, éventuellement, un équipement de support. Aujourd’hui cet équipement de support peut être une pompe ; demain la relation devra évoluer vers une liste générique de dépendances de workflow.

## Stratégie de stockage

La SD doit devenir la source principale pour les configurations structurées et évolutives :

- fichiers lisibles et sauvegardables ;
- capacité largement supérieure ;
- schémas JSON versionnés ;
- import/export et diagnostic facilités.

La NVS reste utile comme repli minimal :

- démarrage possible sans SD ;
- conservation d’un dernier profil validé ;
- valeurs sûres si la SD est absente ou corrompue.

Ordre de chargement retenu :

1. SD ;
2. NVS de secours ;
3. profil sûr vide.

Lors d’une sauvegarde, la SD est écrite en premier, puis la NVS reçoit une copie de secours si elle est disponible.

## Périmètre du Run 6.21

Ce run ajoute :

- `EquipmentAutomationProfile` ;
- le catalogue générique d’équipements ;
- les dépendances de zone ;
- la conversion vers `EquipmentModel` ;
- le contrat abstrait `IEquipmentConfigStorage` ;
- `EquipmentConfigRepository` avec stockage principal et fallback.

Il ne remplace pas encore le store pompe historique du Run 6.20 et n’ajoute aucune commande matérielle. Le prochain run implémentera le backend SD et la migration du format pompe vers le profil générique.
