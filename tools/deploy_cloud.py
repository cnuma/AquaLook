#!/usr/bin/env python3
"""Deploie le service cloud (cloud/php-mutualized) sur AlwaysData DEPUIS GIT.

Pourquoi : jusqu'au 11 oct. 2026 le service etait mis a jour en editant le
serveur ou en recopiant a la main la copie de travail. Rien ne garantissait que
le serveur corresponde a un commit (checkpoint 679eb6b, §7 "derive de config").
Ce script publie le contenu EXACT d'un commit (blobs Git, octet pour octet) et
relit ensuite le serveur pour prouver l'absence d'ecart.

Trois modes :
  plan      (defaut) hors ligne : liste les fichiers du commit et leur SHA-256.
  verifier  lecture seule sur le serveur : compare chaque fichier a Git.
  publier   televerse les fichiers differents, puis verifie tout.

Le script est lance par le PROPRIETAIRE (la publication sur AlwaysData ne se
fait jamais par l'agent). Identifiants lus, dans l'ordre, dans les variables
d'environnement puis dans le .env a la racine du depot (jamais versionne) :
  AQUALOOK_FTP_HOST      ex. ftp-<compte>.alwaysdata.net
  AQUALOOK_FTP_USER
  AQUALOOK_FTP_PASSWORD
  AQUALOOK_FTP_DIR       repertoire du site sur le serveur (defaut : www)

Garde-fous :
  - refuse de publier si cloud/php-mutualized a des modifications non
    committees (on publierait autre chose que ce qui est dans Git) ;
  - FTPS obligatoire (TLS explicite, canal de donnees chiffre) ;
  - n'efface JAMAIS rien sur le serveur : /web/, /editeur/ et .env y vivent
    hors de ce depot. Les fichiers distants inconnus sont seulement signales ;
  - chaque fichier est envoye sous un nom temporaire (refuse par le
    .htaccess) puis renomme, pour qu'aucune requete ne tombe sur un PHP a
    moitie ecrit ; admin.html et app.html apres leurs scripts, puis
    index.php et .htaccess en dernier.

Exemples :
  python tools/deploy_cloud.py
  python tools/deploy_cloud.py verifier
  python tools/deploy_cloud.py publier
"""
import argparse
import ftplib
import hashlib
import io
import os
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
SRC = 'cloud/php-mutualized'

# Jamais publies : documentation, schemas (importes a la main dans
# phpMyAdmin), routeur du serveur de developpement, modeles de .env.
EXCLUS_NOMS = {'README.md', 'router.php', '.env.example', '.gitignore'}
EXCLUS_SUFFIXES = ('.md', '.sql', '.log')
# Publies en dernier : l'aiguillage general et les regles d'acces ne doivent
# changer qu'une fois toutes leurs dependances en place.
ORDRE_FINAL = ['admin.html', 'app.html', 'index.php', '.htaccess']


def git(*args):
    return subprocess.run(['git', *args], cwd=REPO, check=True,
                          capture_output=True).stdout


def lire_env(nom):
    val = os.environ.get(nom)
    if val:
        return val
    env = REPO / '.env'
    if env.exists():
        for ligne in env.read_text(encoding='utf-8').splitlines():
            if ligne.startswith(nom + '='):
                return ligne.split('=', 1)[1].strip()
    return None


def fichiers_du_commit(rev):
    noms = git('ls-tree', '-r', '--name-only', rev, '--', SRC).decode().split('\n')
    retenus = []
    for chemin in filter(None, noms):
        nom = chemin[len(SRC) + 1:]
        if '/' in nom or nom in EXCLUS_NOMS or nom.endswith(EXCLUS_SUFFIXES):
            continue
        retenus.append(nom)
    retenus.sort(key=lambda n: (n in ORDRE_FINAL, ORDRE_FINAL.index(n) if n in ORDRE_FINAL else 0, n))
    return {nom: git('show', '%s:%s/%s' % (rev, SRC, nom)) for nom in retenus}


def sha(octets):
    return hashlib.sha256(octets).hexdigest()


def connecter(banc):
    hote = lire_env('AQUALOOK_FTP_HOST')
    user = lire_env('AQUALOOK_FTP_USER')
    mdp = lire_env('AQUALOOK_FTP_PASSWORD')
    dossier = lire_env('AQUALOOK_FTP_DIR') or 'www'
    manque = [n for n, v in (('AQUALOOK_FTP_HOST', hote), ('AQUALOOK_FTP_USER', user),
                             ('AQUALOOK_FTP_PASSWORD', mdp)) if not v]
    if manque:
        sys.exit('Identifiants absents (%s) : les ajouter au .env a la racine du depot.'
                 % ', '.join(manque))
    if banc:
        # Banc local uniquement (serveur FTP de test sans TLS) : refuse toute
        # autre adresse, pour qu'un mot de passe reel ne parte jamais en clair.
        if hote not in ('127.0.0.1', 'localhost'):
            sys.exit('--banc n\'est permis que vers 127.0.0.1.')
        ftp = ftplib.FTP()
        ftp.connect(hote, int(lire_env('AQUALOOK_FTP_PORT') or 21), timeout=30)
        ftp.login(user, mdp)
    else:
        ftp = ftplib.FTP_TLS(timeout=30)
        ftp.connect(hote, 21)
        ftp.login(user, mdp)
        ftp.prot_p()
    ftp.cwd(dossier)
    return ftp


def lire_distant(ftp, nom):
    tampon = io.BytesIO()
    try:
        ftp.retrbinary('RETR ' + nom, tampon.write)
    except ftplib.error_perm:
        return None
    return tampon.getvalue()


def comparer(ftp, fichiers):
    """Retourne la liste des fichiers a publier ; affiche un bilan par fichier."""
    a_publier = []
    for nom, octets in fichiers.items():
        distant = lire_distant(ftp, nom)
        if distant is None:
            etat = 'ABSENT'
        elif distant == octets:
            etat = 'identique'
        elif distant.replace(b'\r\n', b'\n') == octets:
            etat = 'fins de ligne CRLF seulement'
        else:
            etat = 'DIFFERENT'
        if etat != 'identique':
            a_publier.append(nom)
        print('  %-14s %-30s %s' % (nom, etat, sha(octets)[:16]))
    try:
        distants = set(ftp.nlst())
    except ftplib.error_perm:
        distants = set()
    inconnus = sorted(n for n in distants - set(fichiers)
                      if n not in ('.', '..', '.env', 'web', 'editeur'))
    if inconnus:
        print('  Presents sur le serveur, hors de ce depot (non touches) : '
              + ', '.join(inconnus))
    return a_publier


def publier(ftp, fichiers, noms):
    for nom in noms:
        # Nom temporaire en .log : le .htaccess DEJA en service refuse *.log.
        # Un suffixe inconnu (.tmp) serait servi en texte brut pendant
        # l'envoi -- code source PHP compris.
        tmp = nom + '.deploy-tmp.log'
        try:
            ftp.storbinary('STOR ' + tmp, io.BytesIO(fichiers[nom]))
            try:
                ftp.rename(tmp, nom)
            except ftplib.error_perm:
                # Serveurs qui refusent d'ecraser par RNTO : supprimer puis renommer.
                ftp.delete(nom)
                ftp.rename(tmp, nom)
        except Exception:
            try:
                ftp.delete(tmp)
            except ftplib.all_errors:
                pass
            raise
        print('  envoye : ' + nom)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('mode', nargs='?', default='plan', choices=['plan', 'verifier', 'publier'])
    ap.add_argument('--rev', default='HEAD', help='commit a publier (defaut : HEAD)')
    ap.add_argument('--banc', action='store_true', help=argparse.SUPPRESS)
    args = ap.parse_args()

    rev = git('rev-parse', '--short', args.rev).decode().strip()
    sale = git('status', '--porcelain', '--', SRC).decode().strip()
    fichiers = fichiers_du_commit(rev)
    print('Commit %s, %d fichiers depuis %s :' % (rev, len(fichiers), SRC))
    if sale:
        print('ATTENTION : modifications non committees dans %s :\n%s' % (SRC, sale))

    if args.mode == 'plan':
        for nom, octets in fichiers.items():
            print('  %-14s %7d octets  %s' % (nom, len(octets), sha(octets)[:16]))
        return 0

    if args.mode == 'publier' and sale:
        sys.exit('Refus : committer d\'abord, pour publier exactement ce qui est dans Git.')

    ftp = connecter(args.banc)
    try:
        print('Etat du serveur :')
        ecarts = comparer(ftp, fichiers)
        if args.mode == 'verifier':
            print('Serveur ALIGNE sur %s.' % rev if not ecarts
                  else '%d fichier(s) differ(ent) de %s.' % (len(ecarts), rev))
            return 0 if not ecarts else 1
        if not ecarts:
            print('Rien a publier.')
            return 0
        print('Publication de %d fichier(s) :' % len(ecarts))
        publier(ftp, fichiers, ecarts)
        print('Verification apres publication :')
        restants = comparer(ftp, fichiers)
        if restants:
            print('ECHEC : %d fichier(s) encore differents.' % len(restants))
            return 1
        print('Serveur ALIGNE sur %s.' % rev)
        return 0
    finally:
        try:
            ftp.quit()
        except Exception:
            ftp.close()


if __name__ == '__main__':
    sys.exit(main())
