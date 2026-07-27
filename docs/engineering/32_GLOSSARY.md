# AquaLook Engineering Reference — Glossaire

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Maturité : D3

## Termes

| Terme | Définition |
|---|---|
| AquaLook | contrôleur d’arrosage local sur ESP32 avec interface LCD et Web |
| Runtime | ensemble des composants actifs et de leur coordination pendant l’exécution |
| Scheduler | composant qui calcule les échéances et demande l’exécution des programmes |
| programme | règle persistée décrivant quand et combien de temps arroser |
| zone | unité logique d’arrosage associée à un équipement commandable |
| équipement | ressource matérielle ou logique décrite par le modèle V4 |
| relais | sortie de commande électrique utilisée pour une vanne, une pompe ou un équipement |
| shadow | représentation logicielle passive d’un état ou d’un scénario, sans activation autonome directe |
| EventLog | journal central des événements du système |
| checkpoint | document autonome décrivant un état Git et fonctionnel validé |
| invariant | propriété qui doit rester vraie malgré les évolutions |
| roadmap | description d’une évolution envisagée, distincte de l’état implémenté |
| ADR | décision d’architecture documentée avec son contexte et ses conséquences |
| NVS | stockage clé-valeur persistant de l’ESP32 utilisé pour la configuration active |
| LittleFS | système de fichiers Flash utilisé principalement pour les ressources Web historiques |
| microSD | stockage amovible optionnel pour ressources, exports et récupération |
| fallback | solution de repli utilisée lorsqu’une source ou un service est indisponible |
| hot-reload | application d’une configuration sans redémarrage |
| redraw | opération de rafraîchissement graphique, ciblée ou complète |
| D0 à D5 | niveaux de maturité documentaire définis dans l’index du manuel |
| mode dégradé | fonctionnement réduit mais sûr lorsque certains composants sont indisponibles |
| validation matérielle | vérification réelle sur l’équipement, distincte de la compilation |
| temps mural | durée observée incluant les pauses d’ordonnancement et attentes système |
| uptime | durée écoulée depuis le démarrage, généralement fondée sur `millis()` |
| heure absolue | date et heure obtenues après synchronisation NTP |
| source de vérité | référence prioritaire utilisée pour trancher une divergence documentaire |
| buildfs | construction de l’image du système de fichiers embarqué depuis `data/` |
| legacy | comportement historique conservé temporairement pour compatibilité ou repli |

## Règle de maintenance

Un terme nouveau ou ambigu utilisé dans plusieurs documents est ajouté ici. Une définition ne doit pas masquer une différence réelle entre le firmware legacy et le backend V4.
