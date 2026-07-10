# AquaLook V4 — Backlog d’architecture — Addendum Run 6.4

**Date :** 10 juillet 2026  
**Run :** Phase 6 — Run 6.4  
**Sujet :** profils compile-time legacy / V4 sans migration de zone

## Décision ajoutée

| ID | Sujet | Statut |
|---|---|---|
| ARCH-104 | Profils PlatformIO explicites legacy / V4 | **Ajoutés, compilation à valider** |

## Décisions du Run 6.4

- le profil nominal `[env:ProgrammeArrosage]` reste explicitement legacy ;
- ajout de `[env:ProgrammeArrosage_legacy]` comme alias hérité du profil nominal ;
- ajout de `[env:ProgrammeArrosage_v4]` comme profil expérimental hérité ;
- macros nominales : `AQUALOOK_RELAY_BACKEND_LEGACY=1` et `AQUALOOK_RELAY_BACKEND_V4=0` ;
- macros V4 : `AQUALOOK_RELAY_BACKEND_LEGACY=0` et `AQUALOOK_RELAY_BACKEND_V4=1` ;
- `build_unflags` retire les macros legacy héritées avant définition des valeurs V4 ;
- aucun changement de `main.cpp` ;
- aucune instance de `V4RelayPhysicalBackend` dans le runtime ;
- aucune zone migrée ;
- aucun changement NVS, Web, LCD ou JSON ;
- backend historique et fallback direct conservés.

## Validation attendue

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
pio run -e ProgrammeArrosage -t upload
if ($LASTEXITCODE -eq 0) {
    pio device monitor -e ProgrammeArrosage
}
```

## Prochaine étape conditionnelle

Le Run 6.5 ne doit commencer qu’après compilation réussie des profils legacy et V4, puis validation matérielle du profil nominal.

```text
AquaLook V4 — Phase 6 — Run 6.5
Activation contrôlée d’une zone pilote
```
