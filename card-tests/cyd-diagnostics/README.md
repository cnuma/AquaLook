# CYD Card Diagnostics

Projet PlatformIO autonome destine uniquement au diagnostic des cartes CYD.

Il ne depend pas du firmware AquaLook et ne pilote ni l'ecran, ni le tactile, ni les relais.

## Informations affichees

- famille et revision de la puce ESP32 ;
- nombre de coeurs et frequence CPU ;
- taille, vitesse et mode de la memoire flash ;
- heap interne totale, libre et plus grand bloc contigu ;
- presence ou absence de PSRAM ;
- taille totale et libre de la PSRAM ;
- test reel d'allocation et d'ecriture de 256 Kio en PSRAM.

## Utilisation dans VS Code / PlatformIO

1. Ouvrir directement le dossier `card-tests/cyd-diagnostics` dans VS Code.
2. Brancher la carte CYD en USB.
3. Modifier le port serie dans `platformio.ini` seulement si PlatformIO ne le detecte pas automatiquement.
4. Executer `PlatformIO: Upload`.
5. Ouvrir le moniteur serie a 115200 bauds.

Commandes equivalentes depuis ce dossier :

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

## Lecture du resultat

La ligne principale est :

```text
PSRAM detectee au runtime      : OUI
```

La detection n'est consideree comme valide que si :

- `psramFound()` retourne vrai ;
- une taille de PSRAM non nulle est annoncee ;
- le test d'allocation de 256 Kio se termine par `OK`.

## Limite d'identification

Une carte CYD ne fournit generalement pas d'identifiant logiciel standard indiquant sa reference commerciale exacte. Le diagnostic identifie donc de maniere fiable le SoC, la flash et la PSRAM, mais la variante precise du PCB doit encore etre confirmee par les inscriptions physiques de la carte et le brochage.

## Isolation Git

Ce projet est maintenu dans la branche `lab/cyd-card-diagnostics`. Cette branche ne doit pas etre fusionnee dans `main` : elle sert de banc de test materiel reutilisable pour comparer les cartes CYD avant leur integration dans AquaLook.
