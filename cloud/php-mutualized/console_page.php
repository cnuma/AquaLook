<?php
/**
 * Console d'administration (admin.html, admin.js) derriere une
 * authentification HTTP Basic (demande du proprietaire, 12 oct. 2026).
 *
 * POURQUOI : sans elle, n'importe qui (humain ou IA) charge la console et lit
 * son code. Le jeton admin protege deja toutes les routes /admin/* ; cette
 * barriere-ci, en amont, cache la console elle-meme a qui n'a pas
 * l'identifiant. Le code reste open source (depot public) : ce qui est
 * protege, c'est l'acces a la page sur CE serveur.
 *
 * POURQUOI EN PHP ET NON AuthType Basic dans .htaccess : AuthUserFile exige
 * le chemin absolu du .htpasswd sur le serveur (propre au compte, a ecrire
 * dans un depot public) et ne se teste pas sans Apache. Ici, la .htaccess
 * reecrit admin.html et admin.js vers index.php, qui les sert apres
 * verification ; le banc local (router.php) fait la meme reecriture.
 *
 * Identifiants dans le .env du serveur, jamais en base ni dans le depot :
 *   ADMIN_PAGE_USER=<identifiant>
 *   ADMIN_PAGE_HASH=<empreinte password_hash, jamais le mot de passe>
 * Absents : la console repond 503 (on ne sert pas une page qu'on devait
 * proteger parce qu'un reglage manque).
 *
 * Les appels de la console a /admin/* restent soumis au jeton porteur :
 * admin.js pose son propre en-tete Authorization: Bearer, et le navigateur
 * n'y substitue pas les identifiants Basic.
 */

declare(strict_types=1);

const CONSOLE_PAGES = [
    '/admin.html' => 'text/html; charset=utf-8',
    '/admin.js'   => 'text/javascript; charset=utf-8',
];

/** Identifiant et mot de passe Basic de la requete, ou null. */
function console_basic_credentials(): ?array
{
    if (isset($_SERVER['PHP_AUTH_USER'], $_SERVER['PHP_AUTH_PW'])) {
        return [(string)$_SERVER['PHP_AUTH_USER'], (string)$_SERVER['PHP_AUTH_PW']];
    }
    // PHP-FPM : seul l'en-tete brut arrive (c'est deja le cas du jeton
    // porteur des routes /admin/*).
    $header = $_SERVER['HTTP_AUTHORIZATION'] ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? '';
    if (!str_starts_with($header, 'Basic ')) {
        return null;
    }
    $decoded = base64_decode(substr($header, 6), true);
    if ($decoded === false || !str_contains($decoded, ':')) {
        return null;
    }
    return explode(':', $decoded, 2);
}

function serve_console_page(string $path): never
{
    header('Cache-Control: no-store');
    $user = env_value('ADMIN_PAGE_USER');
    $hash = env_value('ADMIN_PAGE_HASH');
    if ($user === '' || $hash === '') {
        http_response_code(503);
        header('Content-Type: text/plain; charset=utf-8');
        echo "Console non disponible : ADMIN_PAGE_USER et ADMIN_PAGE_HASH absents du .env du serveur.\n";
        exit;
    }
    $cred = console_basic_credentials();
    // Mot de passe verifie AVANT l'identifiant : la duree de reponse ne dit
    // pas si l'identifiant essaye est le bon.
    $ok = $cred !== null
        && password_verify($cred[1], $hash)
        && hash_equals($user, $cred[0]);
    if (!$ok) {
        http_response_code(401);
        header('WWW-Authenticate: Basic realm="AquaLook console", charset="UTF-8"');
        header('Content-Type: text/plain; charset=utf-8');
        echo "Identification requise.\n";
        exit;
    }
    header('Content-Type: ' . CONSOLE_PAGES[$path]);
    readfile(__DIR__ . $path);
    exit;
}
