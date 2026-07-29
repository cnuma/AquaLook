# AquaLook Developer Guide — Créer un Manager

- Référence : DEV-002
- Statut : actif
- Maturité : D4

## Quand créer un Manager

Créer un Manager lorsqu’un domaine possède un cycle de vie, un état, des invariants et des dépendances propres. Ne pas créer un Manager pour masquer une simple fonction utilitaire.

## Étapes

1. définir mission, propriétaire des données et modes dégradés ;
2. créer `.h` et `.cpp` avec une API minimale ;
3. injecter les dépendances explicitement ;
4. prévoir `begin()` et, si nécessaire, `update()` non bloquant ;
5. instancier et câbler dans `src/main.cpp` ;
6. ajouter diagnostics, EventLog et FaultManager sans secret ;
7. ajouter un banc ou un test reproductible ;
8. compiler legacy et V4 si le composant touche le Runtime ;
9. créer ou mettre à jour la fiche Firmware et le tome Engineering ;
10. consolider le checkpoint.

## Patron recommandé

```cpp
class ExampleManager {
public:
    bool begin(Dependency* dependency);
    void update();
    bool isReady() const;

private:
    Dependency* _dependency = nullptr;
    bool _ready = false;
};
```

## Contraintes

- pas d’attente réseau infinie ;
- pas d’écriture Flash répétée dans `update()` ;
- pas de dépendance cachée vers un singleton mutable ;
- états et erreurs observables ;
- appels idempotents lorsque pertinent ;
- bornes explicites sur buffers et files ;
- comportement défini si une dépendance est absente.

## Validation

- point d’appel réel vérifié ;
- démarrage nominal et dégradé ;
- absence de fuite mémoire ;
- temps d’exécution mesuré si appelé dans `loop()` ;
- documentation de l’ordre d’initialisation ;
- tests des entrées invalides.

## Références

- `docs/firmware/FW-001_main.md`
- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/developer/DEV-005_Bonnes_pratiques_de_developpement.md`
