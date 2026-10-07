# Checkpoint AquaLook — 7 octobre 2026 — scripts éditables en ligne, TODO de l'éditeur

Document de reprise autonome. Il complète `CHECKPOINT_2026-10-06_e4a7456.md`
(qui reste la référence détaillée du chantier cloud) : avec `AGENTS.md` et
`docs/REPRISE_INGENIEUR.md`, il suffit pour reprendre dans un nouveau chat.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche de travail | `feature/cloud-scripts` (poussée sur `origin`) |
| Commit fonctionnel | `fb275e2` (dernier commit de code : `f44465b`) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-07_fb275e2.md`) |
| Version fonctionnelle (`VERSION`) | `5.9.7` (inchangée ; `5.10.0-dev` proposée, à décider) |
| Dernier numéro de réponse du chat source | `AQL-R028` |
| Cible de production | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, COM4 (à reconfirmer) |

Pile de branches (aucune fusionnée) :
`main` → `feat/moteur-de-regles` → `feature/editeur-graphique-scripts` →
`feature/scripts-paralleles` → **`feature/cloud-scripts`**.

## 2. Source de vérité

La branche `feature/cloud-scripts` sur GitHub, au commit de ce document.
Ne pas repartir de `main` ni d'une branche inférieure de la pile.

## 3. Ce qui est validé

Décision **D014** : les scripts et les phrases se modifient depuis l'espace
en ligne par `config.apply` (le serveur propose, le module arbitre, le local
gagne ; pas de signature de bout en bout ; rien ne se lance à distance).
Validé de bout en bout sur `.141` le 6 octobre 2026 : script 1 modifié depuis
`app.html`, appliqué par le module, accusé, révision 49. Détail des 6 étapes,
fichiers et fonctions : `CHECKPOINT_2026-10-06_e4a7456.md` §3 et §4.

## 4. Depuis le checkpoint précédent

Un seul commit, **documentaire** : `fb275e2` ajoute à `ROADMAP.md` la
section « Éditeur graphique des scripts : améliorations demandées » (voir §8).
Aucun fichier de code n'a changé depuis `f44465b`.

## 5. Invariants préservés

Inchangés (voir `CHECKPOINT_2026-10-06_e4a7456.md` §5) : le module revalide
tout bytecode, rien ne se lance à distance, le local gagne, la durée maximale
de sécurité des relais est intacte, seul `config.apply` est accepté du cloud.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | OK à `e4a7456` (RAM 25,8 %, flash 76,6 %) ; non recompilée depuis, sans objet : seuls des fichiers `.md` ont changé |
| buildfs | non requis (`littlefs/` inchangé) |
| Arbre Git | propre au moment de ce document |
| `.141` | firmware équivalent à `3252d82` ; `scripts-schema.html` sur la SD |
| AlwaysData | à jour : `index.php`, `auth.php`, `app.html`, dossier `/editeur/` (5 fichiers) ; ressources Web du module en 5.9.48 |
| Matériel | aucun relais commandé ; script 1 modifié à distance (`attendre 3` → `attendre 1`) |

## 7. Risques et limites

- **Non prouvé sur carte** : commande > 4 Ko, refus d'un script en cours,
  effacement distant, phrases distantes. Le banc
  `tools/cloud_test_scripts_apply.py` les couvre, mais demande le jeton admin
  **de production** : celui de `cloud/php-mutualized/.env` est refusé (401).
- **Pas de signature de bout en bout** (D014) : risque documenté, borné par
  la durée maximale de sécurité.
- **Une commande en vol par module** : un 409 « réessayez dans quelques
  minutes » si un script attend et qu'on modifie le planning.
- **Chaque modification de `data/scripts-schema.html` (ou de ses dépendances)
  doit être recopiée à deux endroits** : la SD du module et `/editeur/` sur
  AlwaysData.
- `git diff --check` signale de faux « espaces en fin de ligne » sur les
  sources stockées en CRLF.
- `dist/alwaysdata` (5.9.49) est périmé.

## 8. TODO — à traiter ensuite

Détail dans `ROADMAP.md`.

**Éditeur graphique des scripts** (demandes des 6-7 octobre) :

1. visuel dans les scripts quand les notifications sont désactivées (l'état
   n'est exposé ni par `/api/scripts` ni par le miroir : à ajouter) ;
2. panneau « Phrases » centré sur la page ;
3. bouton « Fermer » bleu, au niveau de « Enregistrer la bibliothèque » ;
   règle valable pour toute l'interface ;
4. renommer un script ne met pas à jour la liste déroulante ;
5. afficher la gamme « Aqualook Pro » dans l'interface des scripts ;
6. modifier le « texte généré » depuis l'éditeur graphique (condition pour
   supprimer `scripts.html`) ;
7. boucles « Répéter » et « Tant que » en forme de pince, comme Scratch ;
8. « et » / « ou » entre chaque condition, avec priorité mathématique et
   conditions déplaçables, pour « Si » et « Tant que » (chantier de fond).

**Espace en ligne** : dans la grille de `app.html`, un créneau supprimé en
attente de synchronisation reste invisible.

**Autres suites** : prouver la commande > 4 Ko avec le jeton de production ;
régénérer et publier les ressources Web ; décider de la version
(`5.10.0-dev`) ; fusion de la pile vers `main` après validation (CI).

## 9. Procédure exacte de reprise

1. Lire `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`, ce document, puis
   `CHECKPOINT_2026-10-06_e4a7456.md`.
2. Se placer sur la branche :

```powershell
git fetch origin
git checkout feature/cloud-scripts
git pull --ff-only
git log --oneline -3
git status --short
```

3. Demander le port COM courant, puis (chaîne S3) :

```powershell
git diff --check
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

4. Banc logique de l'éditeur :

```powershell
& "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --headless --disable-gpu --allow-file-access-from-files --dump-dom "file:///<chemin-du-depot>/tools/script_schema_test.html"
```

Rappels : avant un flash pendant le soak, `python tools/soak/announce_reboot.py`
puis committer `tools/soak/ledger.json` ; les `.cpp` modifiés par script
gardent leurs fins de ligne CRLF.

## 10. Commandes Git utiles

```powershell
git log --oneline bd21665..HEAD     # tout le chantier cloud-scripts
git show fb275e2 --stat             # la TODO de l'éditeur
git diff --stat bd21665..HEAD
```
