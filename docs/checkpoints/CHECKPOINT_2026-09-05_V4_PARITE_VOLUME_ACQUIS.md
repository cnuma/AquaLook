# Checkpoint — 5 septembre 2026 — Parité V4 : le volume est acquis

**Branche :** `refonte/v4-moteur`
**Banc :** .141 (ESP32-S3, aucun relais raccordé)
**Profil en service :** `ProgrammeArrosage_s3` (backend relais **legacy**)

---

## Ce qui est prouvé

**218 cycles d'arrosage, zéro désaccord.** À chaque ouverture ou fermeture de
zone, le moteur V4 est comparé au legacy sur le résultat *physique* : même
carte, même canal, même état. Les 218 comparaisons concordent.

```
PARITE z4 OUVRE L=0.3/ON  V=0.3/ON  ok=152 ko=0 ACCORD
PARITE z5 OUVRE L=0.4/ON  V=0.4/ON  ok=153 ko=0 ACCORD
PARITE z3 FERME L=0.2/OFF V=0.2/OFF ok=154 ko=0 ACCORD
```

**Le passage de minuit est franchi proprement** — les cycles ont continué à
travers le changement de jour, le planificateur a pris les créneaux du jour
suivant, sans divergence. C'est le seul critère que le volume ne pouvait pas
acheter, et il est acquis.

**Mémoire saine** : 202 Ko libres, plancher jamais descendu sous 110 Ko, après
plusieurs heures à ~70 cycles/heure — bien au-delà d'un usage réel.

## Gate 1 — état

| critère | état |
|---------|------|
| désaccord = 0 | ✅ 218 cycles, aucun |
| ≥ 100 cycles | ✅ acquis |
| ≥ 7 jours continus | ⏳ 0,14 j — court seul |
| aucun redémarrage imputable | ✅ |
| mémoire stable | ✅ |

Il ne reste que **l'endurance**, qui ne s'achète qu'avec du temps.

## Ce qui est prêt pour la suite

- **`ProgrammeArrosage_s3_v4`** : profil S3 + backend V4, **compile** (flash
  75,2 %). Il manquait ; la bascule aurait buté au moment de flasher.
- **Les gates sont concrets** : le backend V4 porte un `_migratedZoneMask`, et
  `V4PilotRuntime` y pose `1 << 0`. La zone 1 seule bascule, les autres restent
  sur le legacy. Gate 2 = flasher `s3_v4` ; Gate 3 = élargir le masque ;
  rollback = reflasher `s3`.
- **Publication 5.9.19** préparée dans `dist/alwaysdata/` (éditeur de câblage
  relais + éditeur E/S).

## Ce qui reste

1. **Endurance** : laisser tourner, ne plus flasher. `python tools/soak/check_soak.py`.
2. **FTP de la 5.9.19** (action utilisateur) — ensuite la mise à jour peut être
   déclenchée.
3. **Gate 2** : flasher `s3_v4`, prouver au série que V4 pilote la zone 1.
4. **Confirmation physique** (action utilisateur) : qu'une vraie électrovanne
   bouge. Le banc n'ayant aucun relais raccordé, **aucun log ne peut l'établir**.

## État du banc

8 zones, notifications ntfy **coupées**, 280 créneaux (40 cycles/jour).
Configuration jetable, restaurable par `tools/soak/restore_planning.py`.
Voir aussi `AQUALOOK_V4_CRITERE_FIABILITE.md` pour le contrat complet.
