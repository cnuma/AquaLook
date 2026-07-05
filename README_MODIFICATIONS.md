# Refactorisation EventLog et alarmes

Base visee :

- depot : `cnuma/AquaLook`
- branche : `refactor/eventlog-centralise`
- commit de depart observe : `11bdaaa57374867268370fa77e12144d6760d9d1`

## Fichiers a remplacer ou ajouter

- `src/FaultManager.h` : nouveau
- `src/FaultManager.cpp` : nouveau
- `src/EventLog.h` : remplacement
- `src/RelaisManager.h` : remplacement
- `src/RelaisManager.cpp` : remplacement
- `src/WiFiManager.cpp` : remplacement
- `src/WebManager.h` : remplacement
- `src/main.cpp` : remplacement
- `include/config.h` : remplacement

## Comportement LED

- erreur non acquittee : rouge 500 ms / eteint 500 ms ;
- erreur active acquittee : fonction normale avec rappel rouge de 300 ms toutes les 5 s ;
- erreur resolue et acquittee : fonction normale ;
- l'alarme rouge est prioritaire.

Broches retenues pour la CYD ESP32-2432S028R :

- rouge : GPIO 4 ;
- vert : GPIO 16 ;
- bleu : GPIO 17 ;
- logique active LOW.

## Page Web

Nouvelle page :

```text
/logs
```

Routes :

```text
POST /api/logs/ack
GET  /api/faults
```

L'acquittement ne vide pas le journal.

## Compilation

Etat : non compile par l'assistant.

Commandes :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t upload
```

Aucune ressource `data/` n'est modifiee ; `buildfs` n'est donc pas requis.

## Tests materiels

1. demarrer sans expandeur I2C ;
2. verifier le clignotement rouge continu ;
3. ouvrir `/logs` et acquitter ;
4. verifier le rappel rouge toutes les 5 secondes ;
5. reconnecter l'expandeur puis redemarrer ;
6. verifier la disparition du rappel ;
7. verifier le fonctionnement normal precedent de la LED RGB.
