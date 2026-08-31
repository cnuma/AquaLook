<?php
/**
 * API HTTP AquaLook -- version hebergement mutualise (PHP + MySQL/MariaDB).
 *
 * Meme contrat de routes que cloud/api (FastAPI, piste VPS) : le firmware
 * n'a pas a savoir laquelle des deux implementations repond. Execution par
 * requete, comme le veut un hebergement mutualise -- pas de processus
 * permanent, coherent avec la trajectoire retenue le 18 aout 2026
 * (docs/architecture/SYSTEM_ARCHITECTURE.md Sec.5.0).
 *
 * La logique metier d'arrosage n'est PAS ici : l'ESP32 reste l'autorite
 * locale, ce service transporte, historise et met en attente.
 */

declare(strict_types=1);

require_once __DIR__ . '/db.php';

const PROTO_VERSION = 'v1';
const MAX_PAYLOAD_BYTES = 64 * 1024;
// 'config' : instantane de la configuration effective du module (creneaux et
// reglages systeme). Miroir en lecture seule -- le module reste l'autorite,
// voir SYSTEM_ARCHITECTURE.md Sec.9 invariants #1, #2, #3.
const VALID_MSG_TYPES = ['status', 'state', 'event', 'diag', 'config'];
const MODULE_ID_PATTERN = '/^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/';
// Plancher d'entropie d'un jeton fourni. 32 caracteres, soit au moins
// 128 bits s'il est hexadecimal -- hors de portee de la force brute.
const MIN_TOKEN_LENGTH = 32;

header('Content-Type: application/json; charset=utf-8');

function send_json(int $status, array $body): never
{
    http_response_code($status);
    echo json_encode($body, JSON_UNESCAPED_UNICODE);
    exit;
}

function bearer_token(): string
{
    $header = $_SERVER['HTTP_AUTHORIZATION'] ?? '';
    if (!str_starts_with($header, 'Bearer ')) {
        send_json(401, ['detail' => 'jeton porteur manquant']);
    }
    return trim(substr($header, 7));
}

/** Resout le jeton en identifiant de module. Refus sur, jamais silencieux --
 * SYSTEM_ARCHITECTURE.md Sec.7 exige un refus explicite en cas de message
 * incomplet, invalide ou incompatible. */
function require_module(): string
{
    $moduleId = module_id_for_token(bearer_token());
    if ($moduleId === null) {
        send_json(401, ['detail' => 'jeton porteur invalide']);
    }
    return $moduleId;
}

function require_admin(): void
{
    $adminToken = getenv('ADMIN_TOKEN') ?: '';
    if ($adminToken === '') {
        send_json(503, ['detail' => 'ADMIN_TOKEN non configure cote serveur']);
    }
    if (!hash_equals($adminToken, bearer_token())) {
        send_json(401, ['detail' => 'jeton admin invalide']);
    }
}

function read_json_body(): array
{
    $raw = file_get_contents('php://input');
    $data = json_decode($raw ?: '', true);
    if (!is_array($data)) {
        send_json(400, ['detail' => 'corps JSON invalide']);
    }
    return $data;
}

function check_payload_size(array $payload): void
{
    if (strlen(json_encode($payload, JSON_UNESCAPED_UNICODE)) > MAX_PAYLOAD_BYTES) {
        send_json(413, ['detail' => 'charge utile trop volumineuse']);
    }
}

// ── Routage ──────────────────────────────────────────────────────────────────

$method = $_SERVER['REQUEST_METHOD'];
$path = rtrim(parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH) ?: '/', '/') ?: '/';

try {
    if ($method === 'GET' && $path === '/health') {
        send_json(200, ['ok' => true]);
    }

    // ── Routes module (jeton porteur par module) ────────────────────────────

    if ($method === 'POST' && $path === '/v1/report') {
        $moduleId = require_module();
        $body = read_json_body();

        $msgType = $body['type'] ?? null;
        if (!in_array($msgType, VALID_MSG_TYPES, true)) {
            send_json(400, ['detail' => 'type invalide, attendu parmi ' . implode(', ', VALID_MSG_TYPES)]);
        }
        $payload = $body['payload'] ?? null;
        if (!is_array($payload)) {
            send_json(400, ['detail' => 'payload doit etre un objet']);
        }
        check_payload_size($payload);

        // Valide AVANT d'atteindre la base : un objet ou un tableau ici
        // provoquait une TypeError sur insert_message(), donc une reponse 500
        // pour une requete simplement malformee. Un refus explicite en 400
        // est la seule reponse correcte (SYSTEM_ARCHITECTURE.md Sec.7).
        $correlationId = $body['correlationId'] ?? null;
        if ($correlationId !== null && !is_string($correlationId)) {
            send_json(400, ['detail' => 'correlationId doit etre une chaine']);
        }
        if (is_string($correlationId) && strlen($correlationId) > 64) {
            send_json(400, ['detail' => 'correlationId trop long (64 max)']);
        }
        insert_message($moduleId, PROTO_VERSION, $msgType, $correlationId, $payload);
        $firmware = in_array($msgType, ['status', 'state'], true) ? ($payload['firmware'] ?? null) : null;
        touch_module($moduleId, $firmware);
        send_json(200, ['ok' => true]);
    }

    if ($method === 'GET' && $path === '/v1/pending-command') {
        $moduleId = require_module();
        $result = next_pending_command($moduleId);
        send_json(200, $result ?? ['correlationId' => null, 'command' => null]);
    }

    if ($method === 'POST' && $path === '/v1/command/ack') {
        $moduleId = require_module();
        $body = read_json_body();

        $correlationId = $body['correlationId'] ?? null;
        $state = $body['state'] ?? null;
        if (!$correlationId || !in_array($state, ['accepted', 'refused', 'failed'], true)) {
            send_json(400, ['detail' => 'correlationId et state (accepted|refused|failed) requis']);
        }
        $result = $body['result'] ?? null;
        if ($result !== null && !is_array($result)) {
            send_json(400, ['detail' => 'result doit etre un objet si present']);
        }
        // Meme plafond que les remontees. Il manquait ici : un module
        // compromis ou fautif pouvait stocker un resultat de taille
        // arbitraire (200 Ko acceptes en test le 29 aout 2026), alors que
        // /v1/report etait plafonne a 64 Kio.
        if ($result !== null) {
            check_payload_size($result);
        }

        $finalState = settle_command($moduleId, $correlationId, $state, $result);
        if ($finalState === null) {
            send_json(404, ['detail' => 'correlationId inconnu pour ce module']);
        }
        send_json(200, ['ok' => true, 'state' => $finalState]);
    }

    // ── Routes admin (jeton admin) ──────────────────────────────────────────

    // La console d'administration est un fichier statique servi par Apache
    // (.htaccess ne reecrit que ce qui n'existe pas sur disque). Cette route
    // n'existe que pour que /admin, tape a la main, aboutisse quand meme.
    if ($method === 'GET' && $path === '/admin') {
        header('Content-Type: text/html; charset=utf-8');
        header('Location: /admin.html', true, 302);
        exit;
    }

    if ($method === 'GET' && $path === '/admin/modules') {
        require_admin();
        send_json(200, list_modules());
    }

    if ($method === 'GET' && $path === '/admin/messages') {
        require_admin();
        $moduleId = $_GET['moduleId'] ?? '';
        if (!preg_match(MODULE_ID_PATTERN, $moduleId)) {
            send_json(400, ['detail' => 'moduleId invalide']);
        }
        $type = $_GET['type'] ?? null;
        if ($type !== null && !in_array($type, VALID_MSG_TYPES, true)) {
            send_json(400, ['detail' => 'type invalide']);
        }
        send_json(200, list_messages($moduleId, (int)($_GET['limit'] ?? 50), $type));
    }

    if ($method === 'GET' && $path === '/admin/commands') {
        require_admin();
        $moduleId = $_GET['moduleId'] ?? '';
        if (!preg_match(MODULE_ID_PATTERN, $moduleId)) {
            send_json(400, ['detail' => 'moduleId invalide']);
        }
        send_json(200, list_commands($moduleId, (int)($_GET['limit'] ?? 50)));
    }

    // Revision de configuration courante, telle que le module l'a remontee.
    // La console s'en sert pour renseigner baseRevision sans saisie manuelle
    // (voir latest_config() dans db.php).
    if ($method === 'GET' && $path === '/admin/config') {
        require_admin();
        $moduleId = $_GET['moduleId'] ?? '';
        if (!preg_match(MODULE_ID_PATTERN, $moduleId)) {
            send_json(400, ['detail' => 'moduleId invalide']);
        }
        $latest = latest_config($moduleId);
        if ($latest === null) {
            send_json(404, ['detail' => 'aucun instantane de configuration recu de ce module']);
        }
        send_json(200, $latest);
    }

    if ($method === 'POST' && $path === '/admin/command/cancel') {
        require_admin();
        $body = read_json_body();
        $correlationId = $body['correlationId'] ?? '';
        if (!is_string($correlationId) || $correlationId === '' || strlen($correlationId) > 64) {
            send_json(400, ['detail' => 'correlationId requis']);
        }
        $state = cancel_command($correlationId);
        if ($state === null) {
            send_json(404, ['detail' => 'correlationId inconnu']);
        }
        // 409 si la commande etait deja reglee : l'annulation n'a rien fait,
        // et le dire evite de laisser croire qu'on a rattrape une commande
        // que le module a deja appliquee.
        if ($state !== 'expired') {
            send_json(409, ['detail' => 'commande deja reglee', 'state' => $state]);
        }
        send_json(200, ['ok' => true, 'state' => $state]);
    }

    if ($method === 'POST' && $path === '/admin/module-token') {
        require_admin();
        $body = read_json_body();
        $moduleId = $body['moduleId'] ?? '';
        if (!preg_match(MODULE_ID_PATTERN, $moduleId)) {
            send_json(400, ['detail' => 'moduleId invalide']);
        }
        $token = $body['token'] ?? bin2hex(random_bytes(32));
        // Le hachage protege la base, pas un jeton faible : une empreinte de
        // "1234" se retrouve par simple dictionnaire. On impose donc un
        // plancher d'entropie a tout jeton fourni par l'administrateur. Celui
        // genere ici en fait 64 (32 octets aleatoires en hexadecimal).
        if (strlen($token) < MIN_TOKEN_LENGTH) {
            send_json(400, ['detail' => 'jeton trop court, ' . MIN_TOKEN_LENGTH . ' caracteres minimum']);
        }
        if (strlen($token) > 128) {
            send_json(400, ['detail' => 'jeton trop long (128 max)']);
        }
        try {
            upsert_module_token($moduleId, $token, $body['label'] ?? null);
        } catch (PDOException $e) {
            // 23000 = violation de contrainte : le jeton appartient deja a un
            // autre module (UNIQUE sur module_token.token). C'est une erreur
            // de l'appelant, pas une panne du serveur - repondre 500 la
            // rendait indiscernable d'un incident.
            if ($e->getCode() === '23000') {
                send_json(409, ['detail' => 'jeton deja attribue a un autre module']);
            }
            throw $e;
        }
        // Montre le jeton une seule fois, en clair -- a noter cote
        // administrateur, jamais relisible depuis le serveur ensuite.
        send_json(200, ['moduleId' => $moduleId, 'token' => $token]);
    }

    if ($method === 'POST' && $path === '/admin/command') {
        require_admin();
        $body = read_json_body();
        $moduleId = $body['moduleId'] ?? '';
        if (!preg_match(MODULE_ID_PATTERN, $moduleId)) {
            send_json(400, ['detail' => 'moduleId invalide']);
        }
        $command = $body['command'] ?? null;
        if (!is_array($command)) {
            send_json(400, ['detail' => 'command doit etre un objet']);
        }
        check_payload_size($command);

        $correlationId = create_command($moduleId, $command, $body['issuedBy'] ?? null);
        send_json(200, ['correlationId' => $correlationId]);
    }

    send_json(404, ['detail' => 'route inconnue']);
} catch (Throwable $e) {
    // Jamais de trace technique renvoyee au client -- refus sur plutot que
    // fuite d'information (SYSTEM_ARCHITECTURE.md Sec.7).
    error_log('AquaLook API erreur: ' . $e->getMessage());
    send_json(500, ['detail' => 'erreur interne']);
}
