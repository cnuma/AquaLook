#!/usr/bin/env python3
"""Session Web locale du module pour les outils de banc (D016, lot F).

Depuis le lot F, toute ecriture sur le module exige une session, ouverte
par defi-reponse : le module donne un defi, l'outil repond
HMAC-SHA256(mot de passe, "session|" + defi), le module pose un cookie.
Le mot de passe ne circule jamais.

Le mot de passe Web (secret ApiAuth) est lu, dans l'ordre :
  1. --password
  2. variable d'environnement AQUALOOK_WEB_PASSWORD
  3. ligne AQUALOOK_WEB_PASSWORD=... du fichier .env a la racine du depot
     (ignore par Git -- le depot est public, jamais de secret committe)
Un module sans mot de passe pose n'en demande pas.

Bibliotheque :
    from module_session import open_session
    s = open_session('192.168.1.141')
    s.post_json('/api/dayslot', {...})

En ligne de commande :
    python tools/module_session.py state
    python tools/module_session.py login
    python tools/module_session.py post /api/restart '{}'
    python tools/module_session.py deploy data/index.html data/session.js
"""
import argparse
import hashlib
import hmac
import http.cookiejar
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_HOST = '192.168.1.141'


def read_password(explicit=None):
    if explicit:
        return explicit
    env = os.environ.get('AQUALOOK_WEB_PASSWORD')
    if env:
        return env
    path = os.path.join(ROOT, '.env')
    try:
        with open(path, encoding='utf-8') as fh:
            for line in fh:
                key, sep, value = line.strip().partition('=')
                if sep and key.strip() == 'AQUALOOK_WEB_PASSWORD':
                    return value.strip().strip('"').strip("'")
    except OSError:
        pass
    return None


class ModuleSession:
    def __init__(self, host, timeout=10):
        self.base = 'http://%s' % host
        self.timeout = timeout
        self.jar = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(
            urllib.request.HTTPCookieProcessor(self.jar))

    def request(self, path, data=None, content_type=None, method=None):
        headers = {'Content-Type': content_type} if content_type else {}
        req = urllib.request.Request(self.base + path, data=data,
                                     headers=headers, method=method)
        try:
            with self.opener.open(req, timeout=self.timeout) as r:
                return r.status, r.read()
        except urllib.error.HTTPError as exc:
            return exc.code, exc.read()

    def get_json(self, path):
        status, body = self.request(path)
        try:
            return status, json.loads(body or b'{}')
        except ValueError:
            # Firmware anterieur au lot F : 404 en texte brut.
            return status, {'raw': body.decode('utf-8', 'replace')}

    def post_json(self, path, payload):
        status, body = self.request(path, json.dumps(payload).encode(),
                                    'application/json', 'POST')
        try:
            return status, json.loads(body or b'{}')
        except ValueError:
            return status, {'raw': body.decode('utf-8', 'replace')}

    def login(self, password):
        status, ch = self.get_json('/api/session/challenge')
        if status != 200:
            raise RuntimeError('defi refuse : HTTP %s' % status)
        if not ch.get('configure'):
            return 'aucun mot de passe pose : module ouvert'
        if not password:
            raise RuntimeError('mot de passe Web absent (--password, '
                               'AQUALOOK_WEB_PASSWORD ou .env)')
        sig = hmac.new(password.encode('utf-8'),
                       ('session|' + ch['nonce']).encode('ascii'),
                       hashlib.sha256).hexdigest()
        status, out = self.post_json('/api/session/login',
                                     {'nonce': ch['nonce'], 'sig': sig})
        if status != 200:
            raise RuntimeError('session refusee : HTTP %s %s' % (status, out))
        return 'session ouverte'


def open_session(host=DEFAULT_HOST, password=None):
    s = ModuleSession(host)
    s.login(read_password(password))
    return s


def install_global_session(host=DEFAULT_HOST, password=None):
    """Ouvre une session et l'installe pour tout urllib.request.urlopen.

    Pour les outils existants (soak) : une ligne suffit, leurs appels
    urlopen() emportent ensuite le cookie de session sans autre changement.
    """
    s = open_session(host, password)
    urllib.request.install_opener(s.opener)
    return s


def deploy(s, files):
    """Depot transactionnel sur la SD : transit, fichiers, bascule."""
    status, body = s.request('/api/debug/deploy-begin', b'', None, 'POST')
    if status != 200:
        raise RuntimeError('deploy-begin : HTTP %s %s' % (status, body))
    for path in files:
        name = os.path.basename(path)
        with open(path, 'rb') as fh:
            data = fh.read()
        status, body = s.request(
            '/api/debug/deploy-file?name=' + urllib.parse.quote(name),
            data, 'application/octet-stream', 'POST')
        print('  %-24s %6d o  HTTP %s' % (name, len(data), status))
        if status != 200:
            raise RuntimeError('deploy-file %s : %s' % (name, body))
    status, body = s.request('/api/debug/deploy-commit', b'', None, 'POST')
    if status != 200:
        raise RuntimeError('deploy-commit : HTTP %s %s' % (status, body))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--host', default=DEFAULT_HOST)
    ap.add_argument('--password', help='sinon AQUALOOK_WEB_PASSWORD ou .env')
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('state', help='etat de session (sans se connecter)')
    sub.add_parser('login', help='ouvre une session et l affiche')
    p = sub.add_parser('post', help='POST JSON sous session')
    p.add_argument('path')
    p.add_argument('json', nargs='?', default='{}')
    d = sub.add_parser('deploy', help='depose des fichiers sur la SD')
    d.add_argument('files', nargs='+')
    args = ap.parse_args()

    s = ModuleSession(args.host)
    if args.cmd == 'state':
        print(json.dumps(s.get_json('/api/session/state')[1]))
        return 0
    try:
        print(s.login(read_password(args.password)))
        if args.cmd == 'login':
            print(json.dumps(s.get_json('/api/session/state')[1]))
        elif args.cmd == 'post':
            status, out = s.post_json(args.path, json.loads(args.json))
            print('HTTP %s %s' % (status, json.dumps(out)))
            return 0 if status < 300 else 1
        elif args.cmd == 'deploy':
            deploy(s, args.files)
            print('depot termine')
    except RuntimeError as exc:
        print('ECHEC : %s' % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
