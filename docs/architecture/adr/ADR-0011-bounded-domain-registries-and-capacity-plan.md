# ADR-0011 — Registres bornés et plan de capacité du domaine

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.7

## Contexte

Les modèles de Phase 1 sont compacts, mais leur nombre d’instances doit rester borné et mesurable sur ESP32. Les registres runtime ne doivent pas utiliser de `std::vector`, de `new` ni de croissance implicite.

## Décision

Un registre générique `BoundedRegistry<T>` est introduit. Il reçoit un tableau externe et une capacité fixe.

Fonctions initiales :

```text
size
capacity
empty
full
data
at
append
findIf
removeAt
clear
```

Le registre ne possède pas le stockage. Sa durée de vie et sa mémoire sont contrôlées par l’appelant.

## Plans de capacité

Trois profils sont définis :

```text
SMALL     16 équipements
STANDARD  32 équipements
EXTENDED  64 équipements
```

Le plan inclut :

- équipements ;
- états runtime ;
- défauts ;
- intentions ;
- exécutions ;
- dépendances ;
- résultats récents ;
- workspace de graphe ;
- arène de paramètres et noms.

## Budgets calculés

```text
SMALL      5 168 octets
STANDARD  10 592 octets
EXTENDED  21 184 octets
```

Ces chiffres couvrent uniquement le domaine V4. Ils excluent :

- stacks FreeRTOS ;
- Wi-Fi et TCP/IP ;
- buffers Web et JSON ;
- affichage et sprites ;
- bibliothèques ;
- fragmentation du heap ;
- configuration candidate supplémentaire éventuelle.

## Seuils verrouillés

```text
SMALL     <= 8 Kio
STANDARD  <= 16 Kio
EXTENDED  <= 32 Kio
```

Les assertions de compilation doivent échouer si une évolution des structures dépasse ces enveloppes.

## Catalogue minimal

Le catalogue initial contient cinq types :

```text
ZONE_VALVE
PUMP
AUXILIARY
GREENHOUSE_VENT
LIGHTING
```

Les descripteurs définissent les capacités requises, les capacités supportées et les bornes de paramètres.

Les validateurs spécifiques au contenu des paramètres restent différés tant que les schémas binaires précis ne sont pas définis.

## Profil recommandé

Le profil `STANDARD` est la cible de conception par défaut.

Il offre :

```text
32 équipements
32 états runtime
32 défauts
32 intentions
16 exécutions concurrentes ou retenues
64 dépendances
32 résultats récents
4 Kio d’arène
```

Le profil `EXTENDED` reste une capacité de réserve, à autoriser uniquement après mesure complète du firmware.

## Options rejetées

### Conteneurs dynamiques

Rejetés : fragmentation et capacité non déterministe.

### Un tableau fixe global par type dans cette phase

Rejeté : le dimensionnement doit rester configurable et testable.

### Catalogue construit dynamiquement

Rejeté : les types de base sont connus à la compilation et peuvent rester en mémoire programme.

## Conséquences

- la consommation du domaine devient calculable avant intégration ;
- les registres spécialisés pourront réutiliser le même contrat ;
- le profil standard reste très inférieur à la RAM disponible d’un ESP32 classique ;
- la compilation PlatformIO et les mesures de heap restent obligatoires avant activation runtime ;
- la Phase 1 peut être clôturée sur le plan des modèles, mais pas encore intégrée au firmware historique.

## Invariants

1. Aucun registre du domaine ne croît dynamiquement.
2. La capacité est connue à la construction.
3. Un dépassement est refusé explicitement.
4. Le profil STANDARD est la référence par défaut.
5. Les budgets du domaine n’incluent pas les couches système et UI.
6. Toute augmentation de taille d’une structure doit réévaluer les trois budgets.
