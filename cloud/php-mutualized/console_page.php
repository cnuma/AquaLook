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

// Echecs de la console comptes dans login_attempt (P1.11, SEC-030), sous un
// identifiant qui ne peut pas etre une adresse mail : aucune migration de
// schema, et la purge de cleanup.php s'applique deja. Par adresse IP ET au
// total, sur la fenetre de l'espace utilisateur (RATE_WINDOW_MIN, auth.php) ;
// plafonds reglables dans la console (CONSOLE_MAX_FAILURES_*). Le plafond
// total ferme la console a tous, proprietaire compris, tant que la fenetre
// glissante en contient trop : choix assume, un mot de passe devine coute
// plus cher (remede : attendre, ou relever le plafond en base).
const CONSOLE_ATTEMPT_KEY = '#console';

/** Echecs recents de la console : [par cette adresse, toutes adresses]. */
function console_recent_failures(string $ip): array
{
    $stmt = db()->prepare(
        'SELECT SUM(ip = ?) AS par_ip, COUNT(*) AS total'
        . ' FROM login_attempt'
        . ' WHERE email = ? AND succeeded = 0'
        . ' AND ts > (UTC_TIMESTAMP(3) - INTERVAL ? MINUTE)'
    );
    $stmt->execute([$ip, CONSOLE_ATTEMPT_KEY, RATE_WINDOW_MIN]);
    $row = $stmt->fetch() ?: [];
    return [(int)($row['par_ip'] ?? 0), (int)($row['total'] ?? 0)];
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
    $ip = client_ip();
    if ($cred !== null) {
        // Essais repetes (P1.11, SEC-030) : verifie AVANT password_verify, un
        // poste bloque ne fait plus travailler bcrypt. Une requete sans
        // identifiant (premier appel du navigateur, qui attend le 401 pour
        // ouvrir la fenetre de saisie) ne compte pas.
        try {
            [$parIp, $total] = console_recent_failures($ip);
        } catch (Throwable $e) {
            // Base injoignable : on ne sert pas une page protegee sans pouvoir
            // compter les essais (la console ne fonctionne pas sans base).
            http_response_code(503);
            header('Content-Type: text/plain; charset=utf-8');
            echo "Console non disponible : base de donnees injoignable.\n";
            exit;
        }
        if ($parIp >= setting_int('CONSOLE_MAX_FAILURES_IP')
            || $total >= setting_int('CONSOLE_MAX_FAILURES_TOTAL')) {
            http_response_code(429);
            header('Retry-After: ' . (RATE_WINDOW_MIN * 60));
            header('Content-Type: text/plain; charset=utf-8');
            echo "Trop d'essais : reessayer dans " . RATE_WINDOW_MIN . " minutes.\n";
            exit;
        }
    }
    // Mot de passe verifie AVANT l'identifiant : la duree de reponse ne dit
    // pas si l'identifiant essaye est le bon.
    $ok = $cred !== null
        && password_verify($cred[1], $hash)
        && hash_equals($user, $cred[0]);
    if (!$ok) {
        if ($cred !== null) {
            record_attempt(CONSOLE_ATTEMPT_KEY, $ip, false);
        }
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
