<?php
/**
 * Comptes, sessions et limitation de debit pour l'espace utilisateur.
 *
 * Distinct des jetons de module (db.php) et du jeton administrateur
 * (index.php) : trois publics, trois mecanismes, trois niveaux de pouvoir.
 * Un module ne peut que parler de lui-meme ; un utilisateur ne voit que ses
 * modules ; l'administrateur voit tout. Les melanger reviendrait a donner a
 * une carte SD posee dans un jardin les droits de l'administrateur.
 */

declare(strict_types=1);

require_once __DIR__ . '/db.php';

const SESSION_COOKIE   = 'aq_session';
const SESSION_DAYS     = 30;
// Fenetre et plafonds de la limitation de debit. Volontairement genereux
// pour un humain qui se trompe, et hors de portee d'une force brute : a
// 10 essais par quart d'heure, essayer un million de mots de passe demande
// trois ans.
const RATE_WINDOW_MIN  = 15;
const RATE_MAX_EMAIL   = 10;
const RATE_MAX_IP      = 30;

function client_ip(): string
{
    // Pas de confiance aveugle a X-Forwarded-For : n'importe qui peut
    // l'envoyer. REMOTE_ADDR est pose par le serveur lui-meme. Sur un
    // hebergement mutualise derriere un frontal, il vaut l'adresse du
    // frontal -- la limitation par adresse y perd de sa finesse, mais celle
    // par identifiant, qui est la vraie protection du compte, reste exacte.
    $ip = $_SERVER['REMOTE_ADDR'] ?? '';
    return is_string($ip) ? substr($ip, 0, 45) : '';
}

/** Nombre d'echecs recents, par identifiant et par adresse. */
function recent_failures(string $email, string $ip): array
{
    $stmt = db()->prepare(
        'SELECT '
        . ' SUM(email = ?) AS par_email,'
        . ' SUM(ip = ?) AS par_ip'
        . ' FROM login_attempt'
        . ' WHERE succeeded = 0 AND ts > (UTC_TIMESTAMP(3) - INTERVAL ? MINUTE)'
    );
    $stmt->execute([$email, $ip, RATE_WINDOW_MIN]);
    $row = $stmt->fetch() ?: [];
    return [(int)($row['par_email'] ?? 0), (int)($row['par_ip'] ?? 0)];
}

function record_attempt(string $email, string $ip, bool $succeeded): void
{
    $stmt = db()->prepare(
        'INSERT INTO login_attempt (ts, email, ip, succeeded) VALUES (?, ?, ?, ?)'
    );
    $stmt->execute([utc_now(), $email !== '' ? $email : null, $ip !== '' ? $ip : null, $succeeded ? 1 : 0]);
}

/**
 * Verifie un couple identifiant/mot de passe.
 *
 * Rend l'identifiant du compte, ou null. Ne distingue JAMAIS "compte
 * inconnu" de "mot de passe faux" dans ce qu'elle rend : la difference
 * permettrait d'enumerer les comptes existants.
 *
 * Calcule un password_verify() meme quand le compte n'existe pas, sur une
 * empreinte factice. Sans cela, un compte inconnu repondrait en une
 * fraction du temps d'un compte connu, et la mesure de ce delai suffirait a
 * savoir lesquels existent.
 */
function verify_credentials(string $email, string $password): ?int
{
    $stmt = db()->prepare('SELECT user_id, password_hash FROM app_user WHERE email = ?');
    $stmt->execute([$email]);
    $row = $stmt->fetch();

    if (!$row) {
        // Empreinte jetable, du meme cout que les vraies.
        password_verify($password, '$2y$10$usqrdlLNBRunFwl0jaOvUeuLNBRunFwl0jaOvUeuLNBRunFwl0jaO');
        return null;
    }
    if (!password_verify($password, $row['password_hash'])) {
        return null;
    }
    // Rehachage si le cout par defaut de PHP a change depuis la creation du
    // compte : la mise a niveau se fait a la connexion, sans demander a
    // l'utilisateur de changer son mot de passe.
    if (password_needs_rehash($row['password_hash'], PASSWORD_DEFAULT)) {
        $stmt = db()->prepare('UPDATE app_user SET password_hash = ? WHERE user_id = ?');
        $stmt->execute([password_hash($password, PASSWORD_DEFAULT), (int)$row['user_id']]);
    }
    return (int)$row['user_id'];
}

/** Ouvre une session et rend le jeton en clair, a poser en cookie. */
function open_session(int $userId): string
{
    $token = bin2hex(random_bytes(32));
    $stmt = db()->prepare(
        'INSERT INTO app_session (token_sha256, user_id, created_at, expires_at, last_seen) '
        . 'VALUES (?, ?, ?, (UTC_TIMESTAMP(3) + INTERVAL ? DAY), ?)'
    );
    $stmt->execute([hash('sha256', $token), $userId, utc_now(), SESSION_DAYS, utc_now()]);

    $stmt = db()->prepare('UPDATE app_user SET last_login = ? WHERE user_id = ?');
    $stmt->execute([utc_now(), $userId]);
    return $token;
}

/** Compte associe a la session en cours, ou null. */
function current_user(): ?array
{
    $token = $_COOKIE[SESSION_COOKIE] ?? '';
    if (!is_string($token) || strlen($token) !== 64) {
        return null;
    }
    $stmt = db()->prepare(
        'SELECT u.user_id, u.email, u.label FROM app_session s '
        . 'JOIN app_user u ON u.user_id = s.user_id '
        . 'WHERE s.token_sha256 = ? AND s.expires_at > UTC_TIMESTAMP(3)'
    );
    $stmt->execute([hash('sha256', $token)]);
    $row = $stmt->fetch();
    if (!$row) {
        return null;
    }
    // Trace de vie, utile pour reperer une session oubliee. Ecriture a chaque
    // requete assumee : le volume est celui d'un humain qui navigue, pas
    // celui des modules.
    $upd = db()->prepare('UPDATE app_session SET last_seen = ? WHERE token_sha256 = ?');
    $upd->execute([utc_now(), hash('sha256', $token)]);
    return ['userId' => (int)$row['user_id'], 'email' => $row['email'], 'label' => $row['label']];
}

function close_session(): void
{
    $token = $_COOKIE[SESSION_COOKIE] ?? '';
    if (is_string($token) && strlen($token) === 64) {
        $stmt = db()->prepare('DELETE FROM app_session WHERE token_sha256 = ?');
        $stmt->execute([hash('sha256', $token)]);
    }
}

/**
 * Pose ou retire le cookie de session.
 *
 * HttpOnly : inaccessible au JavaScript, donc inexploitable par une faille
 * d'injection dans une page. Secure : jamais envoye en clair. SameSite
 * Strict : le navigateur ne le joint pas a une requete venue d'un autre
 * site, ce qui ferme la falsification de requete inter-site sans avoir a
 * gerer un jeton anti-CSRF separe.
 */
function set_session_cookie(?string $token): void
{
    setcookie(SESSION_COOKIE, $token ?? '', [
        'expires'  => $token === null ? time() - 3600 : time() + SESSION_DAYS * 86400,
        'path'     => '/',
        'secure'   => true,
        'httponly' => true,
        'samesite' => 'Strict',
    ]);
}

// ── Administration des comptes ─────────────────────────────────────────────
//
// Passer par l'API plutot que par du SQL colle a la main : le mot de passe
// n'a alors jamais d'existence en clair ailleurs que dans le formulaire, et
// personne n'a besoin de savoir manipuler password_hash(). Un INSERT ecrit a
// la main finit tot ou tard avec un mot de passe stocke tel quel.

const MIN_PASSWORD_LENGTH = 10;

/** Cree un compte. Leve une PDOException 23000 si l'adresse existe deja. */
function create_user(string $email, string $password, ?string $label): int
{
    $stmt = db()->prepare(
        'INSERT INTO app_user (email, password_hash, label, created_at) VALUES (?, ?, ?, ?)'
    );
    $stmt->execute([
        $email,
        // PASSWORD_DEFAULT et non un algorithme fige : PHP le fera evoluer, et
        // verify_credentials() rehache a la connexion quand le defaut change.
        password_hash($password, PASSWORD_DEFAULT),
        $label,
        utc_now(),
    ]);
    return (int)db()->lastInsertId();
}

/** Comptes existants. Ne rend JAMAIS les empreintes de mots de passe. */
function list_users(): array
{
    return db()->query(
        'SELECT u.user_id, u.email, u.label, u.created_at, u.last_login, '
        . '(SELECT COUNT(*) FROM module m WHERE m.owner_user_id = u.user_id) AS modules '
        . 'FROM app_user u ORDER BY u.email'
    )->fetchAll();
}

/** Rattache un module a un compte, ou l'en detache si userId vaut null. */
function set_module_owner(string $moduleId, ?int $userId): bool
{
    $stmt = db()->prepare('UPDATE module SET owner_user_id = ? WHERE module_id = ?');
    $stmt->execute([$userId, $moduleId]);
    return $stmt->rowCount() > 0;
}

// ── Modules d'un compte ────────────────────────────────────────────────────

function user_modules(int $userId): array
{
    $stmt = db()->prepare(
        'SELECT m.module_id, m.label, m.firmware, m.last_seen, c.revision, c.updated_at AS config_updated '
        . 'FROM module m LEFT JOIN module_config c ON c.module_id = m.module_id '
        . 'WHERE m.owner_user_id = ? ORDER BY m.module_id'
    );
    $stmt->execute([$userId]);
    return $stmt->fetchAll();
}

/** Vrai si ce module appartient bien a ce compte. Toute route de l'espace
 *  utilisateur doit le verifier AVANT de lire ou d'ecrire quoi que ce soit :
 *  sans cela, changer l'identifiant dans l'URL donnerait acces au jardin du
 *  voisin. */
function user_owns_module(int $userId, string $moduleId): bool
{
    $stmt = db()->prepare('SELECT 1 FROM module WHERE module_id = ? AND owner_user_id = ?');
    $stmt->execute([$moduleId, $userId]);
    return (bool)$stmt->fetch();
}

// ── Configuration courante ─────────────────────────────────────────────────

/** Enregistre l'instantane de configuration remonte par un module. */
function store_module_config(string $moduleId, array $payload): void
{
    $revision = $payload['revision'] ?? null;
    if (!is_int($revision)) {
        return;   // sans revision, l'instantane ne sert a rien : elle est la
                  // reference du verrouillage optimiste.
    }
    $stmt = db()->prepare(
        'INSERT INTO module_config (module_id, revision, payload, updated_at) VALUES (?, ?, ?, ?) '
        . 'ON DUPLICATE KEY UPDATE revision = VALUES(revision), payload = VALUES(payload), '
        . 'updated_at = VALUES(updated_at)'
    );
    $stmt->execute([$moduleId, $revision, json_encode($payload, JSON_UNESCAPED_UNICODE), utc_now()]);
}

/**
 * Commande config.apply encore en attente pour ce module, s'il y en a une.
 *
 * Sert a FUSIONNER plusieurs reglages faits coup sur coup plutot qu'a les
 * empiler. Chaque commande appliquee incremente la revision du module ; celles
 * deja en file portent alors l'ancienne et se font refuser une a une par le
 * verrouillage optimiste. L'utilisateur qui enchaine quatre reglages en voyait
 * un seul survivre -- constate le 3 septembre 2026.
 *
 * Le verrouillage a raison de refuser une commande batie sur un etat perime.
 * Le tort etait de fabriquer cet etat perime nous-memes.
 */
function pending_config_command(string $moduleId): ?array
{
    $stmt = db()->prepare(
        "SELECT correlation_id, command FROM command "
        . "WHERE module_id = ? AND state = 'pending' ORDER BY seq DESC LIMIT 1"
    );
    $stmt->execute([$moduleId]);
    $row = $stmt->fetch();
    if (!$row) {
        return null;
    }
    $cmd = json_decode($row['command'], true);
    if (!is_array($cmd) || ($cmd['type'] ?? '') !== 'config.apply') {
        return null;
    }
    return ['correlationId' => $row['correlation_id'], 'command' => $cmd];
}

/**
 * Fusionne deux listes de zones, la plus recente l'emportant PAR CRENEAU.
 *
 * La granularite compte, et une premiere version l'avait ratee : fusionner
 * par ZONE faisait perdre les reglages precedents de la meme zone sur
 * d'AUTRES jours. Regler jeudi, puis vendredi, puis samedi sur une meme zone
 * ne laissait que samedi -- constate en usage reel le 3 septembre 2026.
 *
 * L'identite d'un creneau est le triplet (zone, jour, rang) en mode jours
 * fixes, et (zone, rang) en mode intervalle. C'est a ce niveau que le plus
 * recent doit l'emporter, pas au-dessus.
 *
 * Les champs simples d'une zone -- nom, mode, intervalle, seuil de pluie --
 * restent remplaces en bloc par les plus recents : ce sont des valeurs
 * uniques, pas des collections.
 */
function merge_zones(array $anciennes, array $nouvelles): array
{
    $parIndex = [];
    foreach ($anciennes as $z) {
        if (is_array($z) && isset($z['i'])) {
            $parIndex[(int)$z['i']] = $z;
        }
    }
    foreach ($nouvelles as $z) {
        if (!is_array($z) || !isset($z['i'])) {
            continue;
        }
        $i = (int)$z['i'];
        $parIndex[$i] = isset($parIndex[$i]) ? merge_zone($parIndex[$i], $z) : $z;
    }
    ksort($parIndex);
    return array_values($parIndex);
}

function merge_zone(array $ancienne, array $nouvelle): array
{
    $out = $ancienne;
    foreach ($nouvelle as $cle => $valeur) {
        if ($cle === 'daySlots' || $cle === 'intervalSlots') {
            continue;   // traites a part, par creneau
        }
        $out[$cle] = $valeur;
    }
    $jours = merge_slots($ancienne['daySlots'] ?? [], $nouvelle['daySlots'] ?? [], true);
    $inter = merge_slots($ancienne['intervalSlots'] ?? [], $nouvelle['intervalSlots'] ?? [], false);

    // Ne pas emettre de tableau vide : le module l'ignorerait, mais une
    // commande qui annonce des creneaux sans en avoir se lit mal.
    if ($jours) { $out['daySlots'] = $jours; } else { unset($out['daySlots']); }
    if ($inter) { $out['intervalSlots'] = $inter; } else { unset($out['intervalSlots']); }
    return $out;
}

/** Fusionne des creneaux par leur identite reelle : (jour, rang) ou (rang). */
function merge_slots(array $anciens, array $nouveaux, bool $avecJour): array
{
    $parCle = [];
    foreach ([$anciens, $nouveaux] as $liste) {
        foreach ($liste as $s) {
            if (!is_array($s) || !isset($s['slot'])) {
                continue;
            }
            $cle = $avecJour
                ? (((int)($s['day'] ?? 0)) . ':' . (int)$s['slot'])
                : (string)(int)$s['slot'];
            $parCle[$cle] = $s;
        }
    }
    ksort($parCle, SORT_NATURAL);
    return array_values($parCle);
}

function module_config(string $moduleId): ?array
{
    $stmt = db()->prepare('SELECT revision, payload, updated_at FROM module_config WHERE module_id = ?');
    $stmt->execute([$moduleId]);
    $row = $stmt->fetch();
    if (!$row) {
        return null;
    }
    return [
        'revision'  => (int)$row['revision'],
        'updatedAt' => $row['updated_at'],
        'payload'   => json_decode($row['payload'], true),
    ];
}
