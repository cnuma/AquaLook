# AquaLook

AquaLook est un contrôleur d’arrosage connecté basé sur ESP32, avec :

- pilotage de zones via expandeur I²C ;
- planification hebdomadaire et par intervalle ;
- blocage météo OpenWeatherMap ;
- interface Web embarquée ;
- écran TFT tactile ;
- configuration persistée en NVS ;
- ressources Web stockées dans LittleFS.

## Base actuelle

- Branche stable : `main`
- Socle Codex basé sur : `a2cf490aa446c7006557f8df62e1f995f6767359`
- Environnement principal PlatformIO : `ProgrammeArrosage`

## Démarrage rapide

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t buildfs
```

Téléversement :

```powershell
pio run -e ProgrammeArrosage -t upload
pio run -e ProgrammeArrosage -t uploadfs
```

## Documentation technique

La documentation destinée aux développeurs et à Codex est dans :

- `AGENTS.md`
- `docs/codex/`

Commencer par `AGENTS.md`.
