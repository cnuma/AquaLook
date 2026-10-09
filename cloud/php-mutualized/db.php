<?php
/**
 * Acces MySQL/MariaDB pour l'API AquaLook (hebergement mutualise).
 *
 * Meme role que cloud/api/app/db.py (SQLite, piste locale) et
 * cloud/bridge/app/main.py::persist() (PostgreSQL, piste MQTT differee) --
 * seul le moteur de connexion change. Voir schema.sql pour la forme des
 * tables.
 */

require_once __DIR__ . '/env.php';

load_env(__DIR__ . '/.env');

function db(): PDO
{
    static $pdo = null;
    if ($pdo !== null) {
        return $pdo;
    }
    $host = env_value('DB_HOST') ?: '127.0.0.1';
    $port = env_value('DB_PORT') ?: '3306';
    $name = env_value('DB_NAME') ?: 'aqualook';
    $user = env_value('DB_USER') ?: '';
    $pass = env_value('DB_PASSWORD') ?: '';

    $dsn = "mysql:host=$host;port=$port;dbname=$name;charset=utf8mb4";
    $pdo = new PDO($dsn, $user, $pass, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
        // Sans plafond, une base injoignable fait attendre la requete jusqu'a
        // max_execution_time -- souvent 30 s -- puis tomber en erreur fatale,
        // hors de portee du try/catch qui aurait su expliquer la panne. Cinq
        // secondes suffisent largement sur un reseau d'hebergeur, et un echec
        // rapide vaut mieux qu'une attente qui finit en 500 muet.
        PDO::ATTR_TIMEOUT => 5,
    ]);
    return $pdo;
}

function utc_now(): string
{
    return (new DateTime('now', new DateTimeZone('UTC')))->format('Y-m-d H:i:s.v');
}

// ── Modules et jetons ───────────────────────────────────────────────────────

/** Empreinte d'un jeton. Seul ce resultat est stocke -- voir la note sur
 * module_token dans schema.sql pour le choix de SHA-256. */
function token_hash(string $token): string
{
    return hash('sha256', $token);
}

function module_id_for_token(string $token): ?string
{
    $stmt = db()->prepare('SELECT module_id FROM module_token WHERE token_sha256 = ?');
    $stmt->execute([token_hash($token)]);
    $row = $stmt->fetch();
    return $row ? $row['module_id'] : null;
}

/**
 * Enregistre le module et son jeton, de facon ATOMIQUE.
 *
 * Les deux INSERT etaient auparavant hors transaction : si le second
 * echouait -- typiquement parce que le jeton est deja attribue a un autre
 * module, la contrainte UNIQUE sur token -- la ligne module restait creee,
 * sans jeton. Un module fantome, impossible a joindre et invisible comme
 * anomalie. Constate en test le 29 aout 2026.
 *
 * Leve une PDOException en cas de conflit ; l'appelant la traduit en 409.
 */
function upsert_module_token(string $moduleId, string $token, ?string $label): void
{
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare(
            'INSERT INTO module (module_id, label, created_at) VALUES (?, ?, ?) '
            . 'ON DUPLICATE KEY UPDATE label = COALESCE(VALUES(label), label)'
        );
        $stmt->execute([$moduleId, $label, utc_now()]);

        $stmt = $pdo->prepare(
            'INSERT INTO module_token (module_id, token_sha256, created_at) VALUES (?, ?, ?) '
            . 'ON DUPLICATE KEY UPDATE token_sha256 = VALUES(token_sha256), created_at = VALUES(created_at)'
        );
        $stmt->execute([$moduleId, token_hash($token), utc_now()]);
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}

function touch_module(string $moduleId, ?string $firmware): void
{
    $stmt = db()->prepare(
        'UPDATE module SET last_seen = ?, firmware = COALESCE(?, firmware) WHERE module_id = ?'
    );
    $stmt->execute([utc_now(), $firmware, $moduleId]);
}

function list_modules(): array
{
    // SELECT * et non une liste de colonnes : hw_id et hw_id_conflict
    // (schema-v5) apparaissent des que la migration est importee, et la
    // console continue de fonctionner tant qu'elle ne l'est pas.
    $stmt = db()->query('SELECT * FROM module ORDER BY module_id');
    return $stmt->fetchAll();
}

/** Forme produite par DeviceIdentity::hwId() cote firmware. */
const HW_ID_PATTERN = '/^aql-[0-9a-f]{12}$/';

/**
 * Lie l'identifiant materiel annonce au module (D016, lot C).
 *
 * Premier rapport qui le porte : hw_id est pose, puis fige. Ensuite, un
 * identifiant different -- ou deja pris par un autre module -- est note
 * dans hw_id_conflict sans refuser le rapport : couper la synchronisation
 * d'un jardin sur un soupcon ferait plus de mal que de bien, et le jeton
 * reste l'authentification. Le bon identifiant efface le conflit.
 *
 * Silencieux si la migration schema-v5 n'est pas encore importee : un
 * rapport ne doit jamais echouer pour une colonne absente.
 */
function bind_module_hw_id(string $moduleId, string $hwId): void
{
    if (!preg_match(HW_ID_PATTERN, $hwId)) {
        return;
    }
    $pdo = db();
    try {
        $stmt = $pdo->prepare('SELECT hw_id, hw_id_conflict FROM module WHERE module_id = ?');
        $stmt->execute([$moduleId]);
        $row = $stmt->fetch();
        if ($row === false) {
            return;
        }
        if ($row['hw_id'] === $hwId) {
            if ($row['hw_id_conflict'] !== null) {
                $pdo->prepare('UPDATE module SET hw_id_conflict = NULL WHERE module_id = ?')
                    ->execute([$moduleId]);
            }
            return;
        }
        if ($row['hw_id'] === null) {
            try {
                $pdo->prepare('UPDATE module SET hw_id = ?, hw_id_conflict = NULL WHERE module_id = ? AND hw_id IS NULL')
                    ->execute([$hwId, $moduleId]);
                return;
            } catch (PDOException $e) {
                if ($e->getCode() !== '23000') {
                    throw $e;
                }
                // Deja lie a un autre module : on tombe dans le conflit.
            }
        }
        if ($row['hw_id_conflict'] !== $hwId) {
            $pdo->prepare('UPDATE module SET hw_id_conflict = ? WHERE module_id = ?')
                ->execute([$hwId, $moduleId]);
            error_log('AquaLook: conflit hw_id module=' . $moduleId . ' annonce=' . $hwId);
        }
    } catch (PDOException $e) {
        // 42S22 : colonne inconnue (schema-v5 pas importe).
        if ($e->getCode() !== '42S22') {
            throw $e;
        }
    }
}

// ── Messages (telemetrie, etats, evenements, diagnostics) ──────────────────

function insert_message(string $moduleId, string $protoVersion, string $msgType,
                         ?string $correlationId, array $payload): void
{
    $stmt = db()->prepare(
        'INSERT INTO module_message (ts, module_id, proto_version, msg_type, correlation_id, payload) '
        . 'VALUES (?, ?, ?, ?, ?, ?)'
    );
    $stmt->execute([
        utc_now(), $moduleId, $protoVersion, $msgType, $correlationId,
        json_encode($payload, JSON_UNESCAPED_UNICODE),
    ]);
}

/**
 * Derniers messages d'un module, du plus recent au plus ancien.
 *
 * Sert la console d'administration. L'index (module_id, ts DESC) de
 * schema.sql couvre exactement cette lecture : pas de tri en memoire, meme
 * quand l'historique d'un module grossit.
 *
 * Le plafond est applique ici et non laisse a l'appelant : une console qui
 * demanderait tout l'historique ferait tomber un hebergement mutualise, ou
 * la memoire PHP par requete est bornee.
 */
function list_messages(string $moduleId, int $limit, ?string $msgType = null): array
{
    $limit = max(1, min($limit, 200));
    $sql = 'SELECT id, ts, msg_type, correlation_id, payload FROM module_message '
         . 'WHERE module_id = ?';
    $args = [$moduleId];
    if ($msgType !== null) {
        $sql .= ' AND msg_type = ?';
        $args[] = $msgType;
    }
    $sql .= ' ORDER BY ts DESC, id DESC LIMIT ' . $limit;

    $stmt = db()->prepare($sql);
    $stmt->execute($args);
    $rows = [];
    foreach ($stmt->fetchAll() as $row) {
        $rows[] = [
            'id' => (int)$row['id'],
            'ts' => $row['ts'],
            'type' => $row['msg_type'],
            'correlationId' => $row['correlation_id'],
            'payload' => json_decode($row['payload'], true),
        ];
    }
    return $rows;
}

/**
 * Dernier instantane de configuration remonte par un module.
 *
 * La console en a besoin pour renseigner baseRevision : le firmware refuse
 * toute commande config.apply dont la baseRevision ne correspond pas a sa
 * revision courante (verrouillage optimiste, CloudSync.cpp). Faire saisir ce
 * nombre a la main serait une invitation a l'erreur - et le refus qui suit
 * est silencieux du point de vue de l'administrateur.
 */
function latest_config(string $moduleId): ?array
{
    $rows = list_messages($moduleId, 1, 'config');
    return $rows ? $rows[0] : null;
}

// ── Commandes ────────────────────────────────────────────────────────────────

function create_command(string $moduleId, array $command, ?string $issuedBy): string
{
    $correlationId = bin2hex(random_bytes(16));
    $stmt = db()->prepare(
        'INSERT INTO command (correlation_id, module_id, issued_at, issued_by, command, state) '
        . "VALUES (?, ?, ?, ?, ?, 'pending')"
    );
    $stmt->execute([
        $correlationId, $moduleId, utc_now(), $issuedBy,
        json_encode($command, JSON_UNESCAPED_UNICODE),
    ]);
    return $correlationId;
}

function next_pending_command(string $moduleId): ?array
{
    $stmt = db()->prepare(
        "SELECT correlation_id, command FROM command "
        . "WHERE module_id = ? AND state = 'pending' "
        // Ordre par la sequence et non par l'horodatage : voir la note sur
        // command.seq dans schema.sql. Deux commandes emises dans la meme
        // seconde s'ordonnaient auparavant au hasard.
        . "ORDER BY seq ASC LIMIT 1"
    );
    $stmt->execute([$moduleId]);
    $row = $stmt->fetch();
    if (!$row) {
        return null;
    }
    return [
        'correlationId' => $row['correlation_id'],
        'command' => json_decode($row['command'], true),
    ];
}

/**
 * Historique des commandes d'un module, de la plus recente a la plus ancienne.
 *
 * Ordonne par seq et non par issued_at, pour la meme raison que
 * next_pending_command() : deux commandes emises dans la meme seconde
 * s'ordonnaient au hasard. La console doit montrer la file exactement dans
 * l'ordre ou le module la consommera, sinon elle ment sur ce qui va se passer.
 */
function list_commands(string $moduleId, int $limit): array
{
    $limit = max(1, min($limit, 200));
    $stmt = db()->prepare(
        'SELECT seq, correlation_id, issued_at, issued_by, command, state, settled_at, result '
        . 'FROM command WHERE module_id = ? ORDER BY seq DESC LIMIT ' . $limit
    );
    $stmt->execute([$moduleId]);
    $rows = [];
    foreach ($stmt->fetchAll() as $row) {
        $rows[] = [
            'seq' => (int)$row['seq'],
            'correlationId' => $row['correlation_id'],
            'issuedAt' => $row['issued_at'],
            'issuedBy' => $row['issued_by'],
            'command' => json_decode($row['command'], true),
            'state' => $row['state'],
            'settledAt' => $row['settled_at'],
            'result' => $row['result'] !== null ? json_decode($row['result'], true) : null,
        ];
    }
    return $rows;
}

/**
 * Annule une commande encore en attente. Retourne l'etat obtenu, ou null si
 * l'identifiant est inconnu.
 *
 * Ne touche jamais une commande deja reglee : le module l'a alors deja
 * appliquee, et reecrire son etat effacerait la trace de ce qui s'est
 * reellement passe. Une commande partie ne se rattrape pas cote serveur.
 */
function cancel_command(string $correlationId, ?string $raison = null): ?string
{
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare('SELECT state FROM command WHERE correlation_id = ? FOR UPDATE');
        $stmt->execute([$correlationId]);
        $row = $stmt->fetch();
        if (!$row) {
            $pdo->commit();
            return null;
        }
        if ($row['state'] !== 'pending') {
            $pdo->commit();
            return $row['state'];
        }
        // La raison est enregistree dans result, au meme endroit que celle
        // d'un refus par le module. Une commande "annulee" sans explication
        // ressemble a un incident vue de l'interface, alors que c'est le plus
        // souvent une fusion avec un reglage plus recent.
        $stmt = $pdo->prepare(
            "UPDATE command SET state = 'expired', settled_at = ?, result = ? "
            . "WHERE correlation_id = ?"
        );
        $stmt->execute([
            utc_now(),
            $raison !== null ? json_encode(['detail' => $raison], JSON_UNESCAPED_UNICODE) : null,
            $correlationId,
        ]);
        $pdo->commit();
        return 'expired';
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}

/**
 * Retourne l'etat final (celui deja enregistre si deja regle -- pas de
 * retraitement, protection contre le rejeu), ou null si l'identifiant est
 * inconnu ou n'appartient pas a ce module.
 */
function settle_command(string $moduleId, string $correlationId, string $state,
                         ?array $result): ?string
{
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare(
            'SELECT state FROM command WHERE correlation_id = ? AND module_id = ? FOR UPDATE'
        );
        $stmt->execute([$correlationId, $moduleId]);
        $row = $stmt->fetch();
        if (!$row) {
            $pdo->commit();
            return null;
        }
        if ($row['state'] !== 'pending') {
            $pdo->commit();
            return $row['state']; // deja regle : no-op
        }
        $stmt = $pdo->prepare(
            'UPDATE command SET state = ?, settled_at = ?, result = ? '
            . 'WHERE correlation_id = ? AND module_id = ?'
        );
        $stmt->execute([
            $state, utc_now(),
            $result !== null ? json_encode($result, JSON_UNESCAPED_UNICODE) : null,
            $correlationId, $moduleId,
        ]);
        $pdo->commit();
        return $state;
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}
