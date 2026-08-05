# Rapport de qualification — Persistance AquaLook

## Identification

- Date :
- Dépôt : `cnuma/AquaLook`
- Branche :
- Commit :
- SHA affiché au boot :
- Version :
- Build :
- Profil compilé Legacy : PASS / FAIL
- Profil compilé V4 : PASS / FAIL
- Profil flashé :
- Port COM :

## État initial

- Configuration historique chargée : OUI / NON
- SSID déclaré présent : OUI / NON
- Mot de passe déclaré présent : OUI / NON
- Nombre de zones :
- Page principale accessible : OUI / NON
- Page OTA accessible : OUI / NON

## Portail captif

- Portail affiché : PASS / FAIL
- Scan Wi-Fi : PASS / FAIL
- SSID sélectionné :
- Mot de passe saisi, sans le consigner : OUI / NON
- Validation HTTP : PASS / FAIL
- Écriture NVS confirmée avant reboot : PASS / FAIL
- Relecture NVS confirmée avant reboot : PASS / FAIL
- Reboot déclenché uniquement après validation : PASS / FAIL

## Redémarrage

- Boot complet : PASS / FAIL
- SSID relu identique : PASS / FAIL
- Mot de passe déclaré présent : PASS / FAIL
- Connexion Wi-Fi : PASS / FAIL
- Adresse IP :
- Page principale accessible : PASS / FAIL

## Persistance fonctionnelle

### Zone 1

- Paramètre modifié :
- Valeur attendue :
- Valeur après reboot :
- Résultat : PASS / FAIL

### Zone 2

- Paramètre modifié :
- Valeur attendue :
- Valeur après reboot :
- Résultat : PASS / FAIL

### Système ou affichage

- Paramètre modifié :
- Valeur attendue :
- Valeur après reboot :
- Résultat : PASS / FAIL

## Contrôle des logs

Absence obligatoire de :

- [ ] `NOT_ENOUGH_SPACE`
- [ ] `Config: ecriture NVS incomplete`
- [ ] bloc NVS invalide
- [ ] taille NVS invalide
- [ ] boucle de reboot
- [ ] crash AsyncTCP
- [ ] faux incident SD

Informations relevées :

- Taille attendue du blob :
- Taille écrite :
- Taille relue :
- Schéma :
- CRC attendu :
- CRC relu :

## Conclusion

- Persistance validée : OUI / NON
- Portail captif validé : OUI / NON
- Accès Web après reboot validé : OUI / NON
- Régression détectée :
- Tests non effectués :
- Décision : REJETER / CANDIDAT / VALIDER
