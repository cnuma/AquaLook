# AquaLook V4 — Phase 6 — Run 6.5A

## Objet

Raccorder `RelaisManager` et le driver XL9535 V4 à une image de registre de sortie unique, sans migration de zone.

## Modifications

- `RelaisManager` reçoit une instance de `Xl9535SharedOutputState` avant `begin()`.
- Au démarrage, chaque carte XL9535 valide initialise l'image partagée avec son état sûr.
- Les commandes historiques modifient désormais le canal dans l'image partagée, puis écrivent cette image sur la carte.
- Avant chaque écriture XL9535, `RelaisManager` relit l'image partagée et resynchronise ses registres locaux.
- Le driver XL9535 V4 utilise la même image partagée lorsqu'un contexte lui est fourni.
- Le driver V4 conserve son comportement précédent si aucun propriétaire partagé n'est injecté.

## Invariants

- aucune zone V4 migrée ;
- `_migratedZoneMask` reste nul ;
- aucun `V4RelayPhysicalBackend` actif dans `main.cpp` ;
- le backend historique reste le seul chemin de commande actif ;
- aucun changement NVS, Web, LCD ou JSON ;
- le fallback historique est conservé.

## Sécurité

Une commande provenant plus tard du backend V4 ne pourra plus réécrire une image de registre indépendante et écraser les voies encore gérées par `RelaisManager`, à condition que le contexte XL9535 V4 reçoive la même instance partagée.

## Validation requise

```powershell
pio run -e ProgrammeArrosage_v4
pio run -e ProgrammeArrosage -t upload
if ($LASTEXITCODE -eq 0) {
    pio device monitor -e ProgrammeArrosage
}
```

Vérifications matérielles :

- boot nominal ;
- absence de défaut relais I2C ;
- commande manuelle successive de plusieurs zones ;
- maintien de l'état d'une zone lorsqu'une autre change ;
- cohérence Web et LCD.
