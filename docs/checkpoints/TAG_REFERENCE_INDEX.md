# AquaLook — Index des tags de référence

Date de création : 8 août 2026

## Objet

Ce document est le registre explicite des tags Git importants du dépôt `cnuma/AquaLook`.

Il distingue :

- les **tags de release**, qui identifient une version logicielle publiée ou préparée ;
- les **tags de checkpoint**, qui figent un état de validation technique ou matérielle ;
- les **tags d'architecture**, qui figent un jalon structurel précis sans signifier que l'ensemble du produit est validé.

Un tag ne doit jamais être interprété comme « version stable complète » sans lire la colonne **Niveau de validation** et la **référence associée**.

## Référence officielle actuelle de reprise

La référence de reprise AquaLook actuellement retenue après retour à une base pré-OTA et revalidation matérielle est :

`checkpoint-recovery-2026-08-08-hw-validated`

Ce tag pointe sur le commit documentaire :

`add49134e8bf49065992b08587ebada1daa4ef4a`

Le firmware effectivement compilé, flashé et testé sur matériel lors de cette campagne reste :

`adc5ff6d9c7e3a2bf335fa5303755dfa068cc7a8`

Le document détaillé de validation est :

`docs/checkpoints/CHECKPOINT_2026-08-08_RECOVERY_PRE_OTA_WIFI_NVS.md`

Cette référence doit être utilisée comme point de retour officiel si un développement ultérieur introduit une régression majeure.

## Table des tags

| Tag | Date | Commit cible | Type | Niveau de validation | Signification / usage |
|---|---|---|---|---|---|
| `checkpoint-recovery-2026-08-08-hw-validated` | 2026-08-08 | `add49134e8bf49065992b08587ebada1daa4ef4a` | Checkpoint matériel de reprise | **VALIDÉ SUR MATÉRIEL — référence officielle actuelle** | Reprise depuis l'état pré-OTA revalidé : boot, WiFi, portail captif, scan, NVS, reconnexion après reboot, Web, NTP, météo, LCD, tactile, persistance et relais Legacy. Le retrait SD à chaud reste une anomalie connue différée. |
| `checkpoint-ota-3.0-download-verified` | 2026-07-30 | `c9bb4c5be98e7e280f07a8b457273eb960fabf83` | Checkpoint OTA | Validé matériellement pour le téléchargement OTA et SHA-256 | Jalon OTA-3.0 : téléchargement distant et vérification SHA-256 validés sur matériel V4. **Historique OTA ; ne pas utiliser comme base de reprise générale après le retour pré-OTA.** |
| `checkpoint-ota-manifest-ok` | 2026-07-29 | `ed2ae561d93744125fde70e2488385931d8b896d` | Checkpoint OTA | Manifest OTA validé sur matériel V4 | Jalon validant la vérification du manifest OTA. **Checkpoint fonctionnel ciblé, pas validation générale du firmware.** |
| `relay-topology-v1` | 2026-07-06 | `52916360cdb5a8ea42d0a8cddfc82e09b2a31186` | Architecture | Socle compilable | Premier socle `RelayTopology` compilable. Ce tag fige un jalon d'architecture relais ; il ne certifie pas à lui seul une validation matérielle complète du produit. |
| `v5.9.0` | 2026-07-28 | `09ff05d526f8899c33401831c116a1d45e8dd40b` | Release OTA | Release fonctionnelle OTA | AquaLook 5.9.0 — manifest OTA et notification de mise à jour. Version historique du chantier OTA. |
| `v5.9.1` | 2026-07-30 | `1b8038bf7a0dff9fe939d7c31f970ee2609d2e9c` | Release OTA | Release de validation du téléchargement OTA | AquaLook 5.9.1 — version utilisée pour la validation du téléchargement OTA. Version historique du chantier OTA. |
| `v5.9.2` | 2026-07-30 | `9a93c1d49f339156cc6f38d0145344a1b179fc21` | Release OTA | Cible de validation OTA | AquaLook 5.9.2 — cible de téléchargement utilisée pour les essais OTA. Version historique du chantier OTA. |

## Règles d'interprétation

### Tag de release `vX.Y.Z`

Un tag de release identifie une version applicative. Il ne signifie pas automatiquement que tous les sous-systèmes ont été revalidés sur matériel.

### Tag `checkpoint-*`

Un checkpoint doit décrire explicitement :

- le commit réellement visé ;
- ce qui a été compilé ;
- ce qui a été flashé ;
- les fonctions réellement testées ;
- les limites et anomalies encore connues ;
- le document de reprise associé lorsque celui-ci existe.

Le nom du checkpoint doit refléter le périmètre validé et ne pas laisser entendre une validation plus large que celle réellement effectuée.

### Tag d'architecture

Un tag d'architecture fige un socle structurel important. Il ne doit pas être utilisé comme point de restauration produit sans validation complémentaire.

## Politique pour les futurs tags

À chaque nouveau tag de référence important :

1. créer le tag uniquement après validation du périmètre annoncé ;
2. utiliser de préférence un tag annoté avec un message explicite ;
3. ajouter immédiatement une ligne dans ce registre ;
4. indiquer le SHA exact du commit cible ;
5. qualifier le niveau de validation : `compilable`, `testé logiciel`, `testé matériel ciblé`, `validé matériel complet` ;
6. préciser si le tag est **actif**, **historique**, **supplanté** ou **à ne pas utiliser comme base de reprise** ;
7. lorsqu'un nouveau checkpoint devient la référence officielle, le signaler dans la section **Référence officielle actuelle de reprise**.

## Commandes utiles

Lister les tags :

```powershell
git tag --list
```

Voir la cible et le message d'un tag annoté :

```powershell
git show checkpoint-recovery-2026-08-08-hw-validated --no-patch
```

Revenir temporairement sur une référence figée pour inspection :

```powershell
git switch --detach checkpoint-recovery-2026-08-08-hw-validated
```

Pour reprendre un développement à partir d'un checkpoint, créer ensuite une nouvelle branche dédiée plutôt que de travailler en `detached HEAD`.
