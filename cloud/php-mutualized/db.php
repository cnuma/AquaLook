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
    $host = getenv('DB_HOST') ?: '127.0.0.1';
    $port = getenv('DB_PORT') ?: '3306';
    $name = getenv('DB_NAME') ?: 'aqualook';
    $user = getenv('DB_USER') ?: '';
    $pass = getenv('DB_PASSWORD') ?: '';

    $dsn = "mysql:host=$host;port=$port;dbname=$name;charset=utf8mb4";
    $pdo = new PDO($dsn, $user, $pass, [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]);
    return $pdo;
}

function utc_now(): string
{
    return (new DateTime('now', new DateTimeZone('UTC')))->format('Y-m-d H:i:s.v');
}

// ── Modules et jetons ───────────────────────────────────────────────────────

function module_id_for_token(string $token): ?string
{
    $stmt = db()->prepare('SELECT module_id FROM module_token WHERE token = ?');
    $stmt->execute([$token]);
    $row = $stmt->fetch();
    return $row ? $row['module_id'] : null;
}

function upsert_module_token(string $moduleId, string $token, ?string $label): void
{
    $pdo = db();
    $stmt = $pdo->prepare(
        'INSERT INTO module (module_id, label, created_at) VALUES (?, ?, ?) '
        . 'ON DUPLICATE KEY UPDATE label = COALESCE(VALUES(label), label)'
    );
    $stmt->execute([$moduleId, $label, utc_now()]);

    $stmt = $pdo->prepare(
        'INSERT INTO module_token (module_id, token, created_at) VALUES (?, ?, ?) '
        . 'ON DUPLICATE KEY UPDATE token = VALUES(token), created_at = VALUES(created_at)'
    );
    $stmt->execute([$moduleId, $token, utc_now()]);
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
    $stmt = db()->query(
        'SELECT module_id, label, firmware, last_seen FROM module ORDER BY module_id'
    );
    return $stmt->fetchAll();
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
        . "ORDER BY issued_at ASC LIMIT 1"
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
