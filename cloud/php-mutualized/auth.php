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

/**
 * Presence et fraicheur d'UN module, memes colonnes que user_modules() --
 * pour que la page de detail montre "vu" (last_seen, tout rapport confondu)
 * separement de "synchronise" (config_updated, seulement quand le contenu a
 * change) avec exactement la meme source que la liste "Mes modules". Deux
 * requetes proches plutot qu'une factorisation : celle-ci filtre par
 * module_id (deja verifie appartenir a l'appelant), l'autre par
 * owner_user_id -- les meler aurait exige un IN() ou une jointure inutile
 * pour un seul module.
 */
function module_presence(string $moduleId): ?array
{
    $stmt = db()->prepare(
        'SELECT m.module_id, m.hw_id, m.label, m.firmware, m.last_seen, c.revision, c.updated_at AS config_updated '
        . 'FROM module m LEFT JOIN module_config c ON c.module_id = m.module_id '
        . 'WHERE m.module_id = ?'
    );
    $stmt->execute([$moduleId]);
    $row = $stmt->fetch();
    return $row ?: null;
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

/**
 * Enregistre l'instantane de configuration remonte par un module.
 *
 * Retourne vrai si le contenu differe de la derniere sauvegarde connue
 * (capture_config_backup) -- l'appelant s'en sert pour decider si ce rapport
 * merite aussi une ligne dans l'historique brut (module_message), voir la
 * note sur insert_message() dans index.php.
 *
 * Le miroir module_config, lui, est TOUJOURS ecrit : c'est lui qui porte la
 * fraicheur ("configuration lue sur le module, vu il y a...") affichee a
 * l'utilisateur, peu importe si le contenu a change depuis le dernier appel.
 */
function store_module_config(string $moduleId, array $payload): bool
{
    $revision = $payload['revision'] ?? null;
    if (!is_int($revision)) {
        return false;   // sans revision, l'instantane ne sert a rien : elle est
                         // la reference du verrouillage optimiste.
    }
    // Un miroir neuf porte des valeurs de variables aussi fraiches que lui :
    // le suivi par la telemetrie (merge_module_variables) reste valable.
    if (isset($payload['scripts']['variables']) && is_array($payload['scripts']['variables'])) {
        $avant = module_config($moduleId);
        if (!empty($avant['payload']['scripts']['variablesSuivies'])) {
            $payload['scripts']['variablesSuivies'] = true;
        }
    }
    $stmt = db()->prepare(
        'INSERT INTO module_config (module_id, revision, payload, updated_at) VALUES (?, ?, ?, ?) '
        . 'ON DUPLICATE KEY UPDATE revision = VALUES(revision), payload = VALUES(payload), '
        . 'updated_at = VALUES(updated_at)'
    );
    $stmt->execute([$moduleId, $revision, json_encode($payload, JSON_UNESCAPED_UNICODE), utc_now()]);

    return capture_config_backup($moduleId, $revision, $payload);
}

/**
 * Reporte dans le miroir les valeurs des variables g1..g16 qu'un rapport
 * "diag" porte (decision D015) : le module ne les y met que lorsque leur CRC
 * a change depuis son dernier envoi confirme. Seules les valeurs du miroir
 * changent -- ni la revision, ni updated_at, ni les sauvegardes : une valeur
 * qui bouge n'est pas un changement de configuration.
 *
 * Pose scripts.variablesSuivies : des lors, l'absence de variables dans un
 * rapport veut dire "rien n'a bouge", et l'editeur en ligne peut dater les
 * valeurs du dernier contact du module plutot que du miroir. Sans miroir
 * portant deja les variables (firmware anterieur), rien n'est ecrit.
 */
function merge_module_variables(string $moduleId, $vars): void
{
    $valeurs = is_array($vars) ? ($vars['valeurs'] ?? null) : null;
    if (!is_array($valeurs) || count($valeurs) > 64) return;
    foreach ($valeurs as $v) {
        if (!is_int($v)) return;   // forme inattendue : ne rien toucher
    }
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare('SELECT payload FROM module_config WHERE module_id = ? FOR UPDATE');
        $stmt->execute([$moduleId]);
        $json = $stmt->fetchColumn();
        $payload = $json === false ? null : json_decode((string)$json, true);
        if (!is_array($payload) || !isset($payload['scripts']['variables'])
            || !is_array($payload['scripts']['variables'])) {
            $pdo->rollBack();
            return;
        }
        foreach ($payload['scripts']['variables'] as $k => $g) {
            $i = is_array($g) ? ($g['i'] ?? null) : null;
            if (is_int($i) && $i >= 1 && $i <= count($valeurs)) {
                $payload['scripts']['variables'][$k]['valeur'] = $valeurs[$i - 1];
            }
        }
        $payload['scripts']['variablesSuivies'] = true;
        $upd = $pdo->prepare('UPDATE module_config SET payload = ? WHERE module_id = ?');
        $upd->execute([json_encode($payload, JSON_UNESCAPED_UNICODE), $moduleId]);
        $pdo->commit();
    } catch (Throwable $e) {
        if ($pdo->inTransaction()) $pdo->rollBack();
        error_log('merge_module_variables: ' . $e->getMessage());
    }
}

/**
 * Nombre d'etats successifs conserves par module, hors sauvegardes epinglees.
 *
 * Vingt changements de configuration couvrent largement une saison d'arrosage,
 * et la table reste sous quelques centaines de kilo-octets par module.
 */
const BACKUP_KEEP = 20;

/**
 * Enregistre l'etat de la configuration s'il differe du dernier connu.
 * Retourne vrai si une ligne a ete ecrite (ou si l'etat de la question n'a
 * pas pu etre determine -- voir plus bas), faux si le contenu etait deja
 * connu et que rien n'a ete ecrit.
 *
 * Appelee a chaque synchronisation, soit une fois par minute. La quasi-totalite
 * des appels ne doivent RIEN ecrire : sans cela, decrire un jardin qui n'a pas
 * bouge couterait 1440 lignes par jour. L'empreinte du payload sert de test.
 *
 * Ne leve jamais : une sauvegarde qui echoue ne doit pas faire echouer la
 * synchronisation du module. Elle est journalisee et l'on continue -- le miroir
 * module_config, lui, a deja ete ecrit.
 */
function capture_config_backup(string $moduleId, int $revision, array $payload): bool
{
    try {
        // Serialisation figee (memes options que le miroir) pour que
        // l'empreinte soit stable d'un appel a l'autre.
        $json = json_encode($payload, JSON_UNESCAPED_UNICODE);
        if ($json === false) return true;   // rien de fiable a comparer : ne pas
                                             // pretendre que c'est identique.
        $hash = hash('sha256', $json);

        $stmt = db()->prepare(
            'SELECT payload_hash FROM module_config_backup WHERE module_id = ? ORDER BY id DESC LIMIT 1'
        );
        $stmt->execute([$moduleId]);
        $dernier = $stmt->fetchColumn();
        if ($dernier !== false && hash_equals((string)$dernier, $hash)) {
            return false;   // rien n'a bouge
        }

        $ins = db()->prepare(
            'INSERT INTO module_config_backup (module_id, revision, payload_hash, payload, captured_at) '
            . 'VALUES (?, ?, ?, ?, ?)'
        );
        $ins->execute([$moduleId, $revision, $hash, $json, utc_now()]);

        prune_config_backups($moduleId);
        return true;
    } catch (Throwable $e) {
        error_log('capture_config_backup: ' . $e->getMessage());
        // Panne de la sauvegarde, pas de l'historique : mieux vaut une ligne
        // brute en trop dans module_message qu'une ligne perdue par une
        // deduplication qu'on n'a pas su trancher.
        return true;
    }
}

/**
 * Ne conserve que les BACKUP_KEEP etats les plus recents, plus les epingles.
 *
 * L'epinglage existe precisement pour cela : sans lui, une sauvegarde de
 * reference disparaitrait apres vingt reglages de creneaux, c'est-a-dire
 * exactement quand on finit par en avoir besoin.
 */
function prune_config_backups(string $moduleId): void
{
    // LIMIT/OFFSET ne prend pas de parametre lie de facon fiable selon que
    // PDO emule ou non les requetes preparees : la borne est un entier
    // constant du code, jamais une valeur venue de l'exterieur.
    $offset = BACKUP_KEEP - 1;
    $stmt = db()->prepare(
        'SELECT id FROM module_config_backup WHERE module_id = ? AND pinned = 0 '
        . 'ORDER BY id DESC LIMIT 1 OFFSET ' . (int)$offset
    );
    $stmt->execute([$moduleId]);
    $seuil = $stmt->fetchColumn();
    if ($seuil === false) return;

    $del = db()->prepare(
        'DELETE FROM module_config_backup WHERE module_id = ? AND pinned = 0 AND id < ?'
    );
    $del->execute([$moduleId, (int)$seuil]);
}

/**
 * Liste des sauvegardes d'un module, SANS leur contenu.
 *
 * Le payload pese environ 2 Ko ; vingt d'un coup alourdiraient la page pour
 * rien, alors qu'on n'en ouvre qu'une a la fois.
 */
function list_config_backups(string $moduleId): array
{
    $stmt = db()->prepare(
        'SELECT id, revision, captured_at, label, pinned, '
        . 'JSON_LENGTH(payload, ' . chr(39) . '$.zones' . chr(39) . ') AS zones '
        . 'FROM module_config_backup WHERE module_id = ? ORDER BY id DESC'
    );
    $stmt->execute([$moduleId]);
    $out = [];
    foreach ($stmt->fetchAll() as $r) {
        $out[] = [
            'id'         => (int)$r['id'],
            'revision'   => (int)$r['revision'],
            'capturedAt' => $r['captured_at'],
            'label'      => $r['label'],
            'pinned'     => (bool)$r['pinned'],
            'zones'      => $r['zones'] === null ? null : (int)$r['zones'],
        ];
    }
    return $out;
}

/** Une sauvegarde complete, contenu compris. */
function config_backup(string $moduleId, int $id): ?array
{
    $stmt = db()->prepare(
        'SELECT id, revision, captured_at, label, pinned, payload '
        . 'FROM module_config_backup WHERE module_id = ? AND id = ?'
    );
    $stmt->execute([$moduleId, $id]);
    $r = $stmt->fetch();
    if ($r === false) return null;
    return [
        'id'         => (int)$r['id'],
        'revision'   => (int)$r['revision'],
        'capturedAt' => $r['captured_at'],
        'label'      => $r['label'],
        'pinned'     => (bool)$r['pinned'],
        'payload'    => json_decode((string)$r['payload'], true),
    ];
}

/** Nomme et/ou epingle une sauvegarde. Rend false si elle n'existe pas. */
function annotate_config_backup(string $moduleId, int $id, ?string $label, ?bool $pinned): bool
{
    $champs = [];
    $args = [];
    if ($label !== null) {
        $champs[] = 'label = ?';
        $args[] = ($label === '' ? null : mb_substr($label, 0, 80));
    }
    if ($pinned !== null) {
        $champs[] = 'pinned = ?';
        $args[] = $pinned ? 1 : 0;
    }
    if ($champs === []) return false;

    // Existence verifiee a part : rowCount() rend 0 quand la valeur ecrite est
    // identique a l'ancienne, ce qui ferait passer un renommage sans effet pour
    // une sauvegarde introuvable.
    $chk = db()->prepare('SELECT 1 FROM module_config_backup WHERE module_id = ? AND id = ?');
    $chk->execute([$moduleId, $id]);
    if ($chk->fetchColumn() === false) return false;

    $args[] = $moduleId;
    $args[] = $id;
    $stmt = db()->prepare(
        'UPDATE module_config_backup SET ' . implode(', ', $champs)
        . ' WHERE module_id = ? AND id = ?'
    );
    $stmt->execute($args);
    return true;
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
 * Nature d'une commande config.apply en attente (decision D014) :
 * 'script:<i>', 'phrases' ou 'zones'.
 *
 * Depuis que scripts et phrases passent par config.apply, une commande en
 * file n'est plus forcement fusionnable avec une autre : le module refuse une
 * commande mixte, et n'accepte qu'un script par commande. Avant de fusionner
 * ou de remplacer, il faut donc savoir ce qui attend.
 */
function pending_kind(array $command): string
{
    if (isset($command['phrases'])) {
        return 'phrases';
    }
    if (isset($command['scripts'][0]['i'])) {
        return 'script:' . (int)$command['scripts'][0]['i'];
    }
    return 'zones';
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

/**
 * Met a jour la revision connue a partir de l'accuse du module.
 *
 * Le module repond "8 champ(s) applique(s), revision=28" quand il accepte une
 * commande. Le serveur, lui, n'apprenait cette nouvelle revision qu'au rapport
 * de configuration suivant -- un cycle plus tard. Entre les deux, toute
 * commande emise portait l'ANCIENNE revision et se faisait refuser en bloc
 * par le verrouillage optimiste, sans qu'aucun de ses reglages ne soit
 * applique. Constate le 3 septembre 2026.
 *
 * L'information circulait deja ; il suffisait de la lire.
 */
function update_revision_from_ack(string $moduleId, ?array $result): void
{
    $detail = is_array($result) ? ($result['detail'] ?? '') : '';
    if (!is_string($detail) || !preg_match('/revision=(\d+)/', $detail, $m)) {
        return;
    }
    $stmt = db()->prepare(
        'UPDATE module_config SET revision = ? WHERE module_id = ? AND revision < ?'
    );
    // revision < ? : ne jamais faire reculer la valeur connue, un accuse
    // tardif ne doit pas ecraser un rapport plus recent.
    $stmt->execute([(int)$m[1], $moduleId, (int)$m[1]]);
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
