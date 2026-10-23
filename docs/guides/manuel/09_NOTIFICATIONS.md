# 9. Notifications

AquaLook peut prévenir votre téléphone, par l'application gratuite
**ntfy**, quand il se passe quelque chose : début ou fin d'arrosage, mise à
jour disponible, souci de carte SD…

Les notifications sont facultatives. Elles demandent Internet.

## Mise en place (10 minutes)

1. **Sur le téléphone** : installez l'application **ntfy** (App Store ou
   Google Play).
2. **Choisissez un sujet privé** : un nom difficile à deviner, par exemple
   `jardin-lyon-7f3k9q`. Ce nom est la seule protection de vos messages sur
   le serveur public : ne prenez pas « aqualook » ou « jardin ».
3. Dans l'application ntfy, **abonnez-vous** à ce sujet (bouton « + »).
4. **Sur les pages du module** : « Paramètres » > « Options avancées du
   module » > « Notifications » :

   | Champ | À saisir |
   |---|---|
   | « Serveur » | `https://ntfy.sh` (conseillé : envoi chiffré) |
   | « Sujet privé » | le même sujet qu'à l'étape 2 (8 caractères au moins ; lettres, chiffres, `-` et `_`) |
   | « Jeton d'accès facultatif » | seulement si votre sujet est protégé par un compte ntfy |

5. Cochez « Activer les notifications », touchez « Enregistrer », puis
   **« Envoyer un test »** : le téléphone reçoit « AquaLook - test
   notification ».

« Réinitialiser sujet et jeton » efface le sujet et le jeton enregistrés.

> Un serveur en `http://` fonctionne aussi, mais le sujet, le contenu et le
> jeton circulent alors **en clair** : à réserver à un serveur ntfy installé
> chez vous.

## Ce que vous recevez

| Titre de la notification | Quand |
|---|---|
| « AquaLook - début d'arrosage » / « fin d'arrosage » | une zone démarre ou s'arrête, **si vous l'avez demandé pour cette zone** (voir ci-dessous) |
| « AquaLook - nouvelle version disponible » | un nouveau programme pour le module est publié |
| « AquaLook - ressources Web disponibles » | de nouvelles pages Web sont publiées |
| « AquaLook - carte SD indisponible » / « recuperation SD en echec » / « carte SD recuperee » | souci de carte SD, puis retour à la normale (chapitre 13) |
| « AquaLook - reglages recus » / « reglages a distance refuses » | des réglages envoyés depuis l'espace en ligne ont été appliqués, ou refusés |
| « AquaLook - retour a la normale » | le module est sorti seul du mode dégradé |
| « AquaLook - message d'un script » | un de vos scripts envoie un message (chapitre 8) |

## Choisir les zones qui notifient

Pour chaque zone : « Paramètres » > « Zones » > ligne de la zone >
« Notifications ntfy » :

- « Notifier au démarrage » ;
- « Notifier à l'arrêt ».

Puis « Enregistrer ». Une zone sans case cochée n'envoie rien pour ses
arrosages ; les autres notifications (mises à jour, carte SD…) ne dépendent
pas de ces cases.

## Si rien n'arrive

- Le sujet est-il **exactement** le même dans l'application et sur le
  module (majuscules comprises) ?
- Les notifications sont-elles activées dans les réglages du téléphone pour
  l'application ntfy ?
- Le module a-t-il Internet ? La page « Sante du module » indique l'état
  des notifications.
