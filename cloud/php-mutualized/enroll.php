<?php
/**
 * Enrolement d'un module par code court (D016, lot D ; modele RFC 8628).
 *
 *   module      POST /v1/enroll/start {hwId, firmware}  -> deviceCode, userCode
 *   LCD         affiche userCode
 *   utilisateur POST /app/module/claim {userCode}       -> rattache le module
 *   module      POST /v1/enroll/poll {deviceCode}       -> pending ... puis jeton
 *
 * Rattacher un module a un compte est un acte de proprietaire : le code ne
 * doit pouvoir etre lu que sur l'ecran du module. Or /v1/enroll/start est
 * public et le hw_id n'est pas secret (c'est la MAC, lisible sur le LAN).
 * D'ou la regle centrale de ce fichier : un module deja rattache, ou qui
 * possede un jeton, ne peut redemander un code qu'en presentant CE jeton.
 * Un module vendu le garde en NVS, le transfert reste donc possible ; un
 * tiers qui ne connait que le hw_id ne peut rien en faire.
 *
 * Restent anonymes les demandes pour un hw_id inconnu ou desenrole. Le pire
 * qu'un tiers y obtienne est de bloquer un enrolement (il n'a jamais le
 * boitier, donc jamais l'arrosage) ; l'administrateur le leve.
 */

declare(strict_types=1);

require_once __DIR__ . '/db.php';
require_once __DIR__ . '/settings.php';
require_once __DIR__ . '/mail.php';

// Ni O/0 ni I/1 : un code se recopie depuis un petit ecran.
const ENROLL_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const ENROLL_USER_CODE_LEN = 8;
const ENROLL_CLAIM_WINDOW_MIN = 15;
// Message unique pour un code faux, expire ou deja servi : les distinguer
// aiderait a balayer l'espace des codes vivants.
const ENROLL_CLAIM_REFUSED = 'code invalide ou expire';

/**
 * Parametres ENROLL_* lus, ou null si l'un manque. Un settings.php plus
 * ancien que ce fichier les rend a 0 : chaque plafond paraissait alors
 * atteint, et le module affichait "trop de demandes" des la premiere
 * (constate sur AlwaysData le 10 oct. 2026). Mieux vaut le dire.
 */
function enroll_settings(): ?array
{
    $noms = ['ENROLL_CODE_TTL_S', 'ENROLL_POLL_INTERVAL_S', 'ENROLL_MAX_PER_HW_HOUR',
             'ENROLL_MAX_PER_IP_HOUR', 'ENROLL_CLAIM_MAX_FAILURES'];
    $v = [];
    foreach ($noms as $n) {
        $v[$n] = setting_int($n);
        if ($v[$n] <= 0) {
            error_log('AquaLook enrolement: parametre ' . $n . ' absent (settings.php a jour ?)');
            return null;
        }
    }
    return $v;
}

const ENROLL_NOT_CONFIGURED = [503, ['detail' => 'enrolement non configure sur le serveur', 'error' => 'not_configured']];

function enroll_new_user_code(): string
{
    $code = '';
    for ($i = 0; $i < ENROLL_USER_CODE_LEN; $i++) {
        $code .= ENROLL_ALPHABET[random_int(0, strlen(ENROLL_ALPHABET) - 1)];
    }
    return $code;
}

/** Forme canonique (majuscules, sans espace ni tiret), ou null. */
function enroll_normalize_user_code(string $raw): ?string
{
    $code = strtoupper(preg_replace('/[\s-]+/', '', $raw) ?? '');
    if (strlen($code) !== ENROLL_USER_CODE_LEN || strspn($code, ENROLL_ALPHABET) !== ENROLL_USER_CODE_LEN) {
        return null;
    }
    return $code;
}

/** Module deja connu pour ce hw_id, avec son proprietaire et l'existence d'un jeton. */
function enroll_module_for_hw(string $hwId, bool $forUpdate = false): ?array
{
    $stmt = db()->prepare(
        'SELECT m.module_id, m.owner_user_id, m.released_owner_user_id, (t.module_id IS NOT NULL) AS has_token '
        . 'FROM module m LEFT JOIN module_token t ON t.module_id = m.module_id '
        . 'WHERE m.hw_id = ?' . ($forUpdate ? ' FOR UPDATE' : '')
    );
    $stmt->execute([$hwId]);
    $row = $stmt->fetch();
    return $row ?: null;
}

/**
 * Demande de code. $bearer : jeton presente par le module, ou null.
 * Rend [statut HTTP, corps].
 */
function enroll_start(string $hwId, ?string $firmware, string $ip, ?string $bearer): array
{
    if (!preg_match(HW_ID_PATTERN, $hwId)) {
        return [400, ['detail' => 'hwId invalide']];
    }
    if ($firmware !== null && ($firmware === '' || strlen($firmware) > 64)) {
        $firmware = null;
    }
    $reglages = enroll_settings();
    if ($reglages === null) {
        return ENROLL_NOT_CONFIGURED;
    }

    $module = enroll_module_for_hw($hwId);
    if ($module !== null && ($module['owner_user_id'] !== null || (int)$module['has_token'] === 1)) {
        $prouve = $bearer !== null && module_id_for_token($bearer) === $module['module_id'];
        if (!$prouve) {
            // 403 et non 401 : le module n'a pas a deviner qu'un jeton
            // manque, il doit savoir que c'est le rattachement existant qui
            // bloque (LCD : "deja rattache, detacher depuis l'espace en ligne").
            return [403, ['detail' => 'module deja rattache : seul ce module, avec son jeton, peut demander un code',
                          'error' => 'already_enrolled']];
        }
    }

    // Pour chaque plafond : le nombre de demandes de l'heure glissante, et
    // dans combien de secondes la plus ancienne en sortira -- c'est le
    // delai a annoncer (exact quand le plafond vient d'etre atteint).
    $stmt = db()->prepare(
        'SELECT SUM(hw_id = ?) AS par_hw, SUM(ip = ?) AS par_ip, '
        . 'TIMESTAMPDIFF(SECOND, UTC_TIMESTAMP(3), MIN(IF(hw_id = ?, created_at, NULL)) + INTERVAL 1 HOUR) AS libre_hw, '
        . 'TIMESTAMPDIFF(SECOND, UTC_TIMESTAMP(3), MIN(IF(ip = ?, created_at, NULL)) + INTERVAL 1 HOUR) AS libre_ip '
        . 'FROM enroll_request WHERE created_at > (UTC_TIMESTAMP(3) - INTERVAL 1 HOUR)'
    );
    $stmt->execute([$hwId, $ip, $hwId, $ip]);
    $row = $stmt->fetch() ?: [];
    $bloque = false;
    $attente = 1;
    if ((int)($row['par_hw'] ?? 0) >= $reglages['ENROLL_MAX_PER_HW_HOUR']) {
        $bloque = true;
        $attente = max($attente, (int)($row['libre_hw'] ?? 0));
    }
    if ($ip !== '' && (int)($row['par_ip'] ?? 0) >= $reglages['ENROLL_MAX_PER_IP_HOUR']) {
        $bloque = true;
        $attente = max($attente, (int)($row['libre_ip'] ?? 0));
    }
    if ($bloque) {
        header('Retry-After: ' . $attente);
        return [429, [
            'detail'     => 'trop de demandes de code : ' . $reglages['ENROLL_MAX_PER_HW_HOUR']
                          . ' par heure au plus, reessayez dans ' . (int)ceil($attente / 60) . ' min',
            'error'      => 'slow_down',
            'retryAfter' => $attente,
            'limit'      => $reglages['ENROLL_MAX_PER_HW_HOUR'],
        ]];
    }

    $ttl = $reglages['ENROLL_CODE_TTL_S'];
    $interval = $reglages['ENROLL_POLL_INTERVAL_S'];
    $deviceCode = bin2hex(random_bytes(32));
    $userCode = enroll_new_user_code();

    $pdo = db();
    $pdo->beginTransaction();
    try {
        // Une seule demande vivante par hw_id : la plus recente remplace les
        // autres (l'utilisateur a rappuye sur "Rattacher").
        $pdo->prepare("UPDATE enroll_request SET state = 'expired' WHERE hw_id = ? AND state IN ('pending', 'approved')")
            ->execute([$hwId]);
        $pdo->prepare(
            'INSERT INTO enroll_request (device_code_sha256, user_code_sha256, hw_id, firmware, ip, created_at, expires_at) '
            . 'VALUES (?, ?, ?, ?, ?, ?, (UTC_TIMESTAMP(3) + INTERVAL ? SECOND))'
        )->execute([hash('sha256', $deviceCode), hash('sha256', $userCode), $hwId, $firmware,
                    $ip !== '' ? $ip : null, utc_now(), $ttl]);
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }

    return [200, [
        'deviceCode' => $deviceCode,
        'userCode'   => substr($userCode, 0, 4) . '-' . substr($userCode, 4),
        'expiresIn'  => $ttl,
        'interval'   => $interval,
    ]];
}

/**
 * Interrogation par le module. Le jeton n'existe qu'ici : tire au premier
 * poll qui suit l'approbation, stocke en empreinte, rendu une seule fois.
 * Le jeton precedent du module (transfert, re-enrolement) est remplace au
 * meme instant -- pas avant, pour qu'un module approuve mais coupe avant
 * son poll garde un jeton valable et puisse recommencer.
 */
function enroll_poll(string $deviceCode): array
{
    if (!preg_match('/^[0-9a-f]{64}$/', $deviceCode)) {
        return [400, ['detail' => 'deviceCode invalide']];
    }
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare(
            'SELECT id, state, module_id, (expires_at <= UTC_TIMESTAMP(3)) AS perime, '
            . 'TIMESTAMPDIFF(MICROSECOND, last_poll_at, UTC_TIMESTAMP(3)) AS depuis_us '
            . 'FROM enroll_request WHERE device_code_sha256 = ? FOR UPDATE'
        );
        $stmt->execute([hash('sha256', $deviceCode)]);
        $req = $stmt->fetch();

        $vivant = $req && in_array($req['state'], ['pending', 'approved'], true) && (int)$req['perime'] === 0;
        if (!$vivant) {
            if ($req && in_array($req['state'], ['pending', 'approved'], true)) {
                $pdo->prepare("UPDATE enroll_request SET state = 'expired' WHERE id = ?")->execute([$req['id']]);
            }
            $pdo->commit();
            return [410, ['status' => 'expired']];
        }
        // Garde-fou contre une boucle sans attente, pas un plafond fin.
        if ($req['depuis_us'] !== null && (int)$req['depuis_us'] < 1000000) {
            $pdo->commit();
            return [429, ['status' => 'slow_down', 'interval' => setting_int('ENROLL_POLL_INTERVAL_S')]];
        }
        $pdo->prepare('UPDATE enroll_request SET last_poll_at = ? WHERE id = ?')->execute([utc_now(), $req['id']]);

        if ($req['state'] === 'pending') {
            $pdo->commit();
            return [200, ['status' => 'pending', 'interval' => setting_int('ENROLL_POLL_INTERVAL_S')]];
        }

        $token = bin2hex(random_bytes(32));
        $pdo->prepare(
            'INSERT INTO module_token (module_id, token_sha256, created_at) VALUES (?, ?, ?) '
            . 'ON DUPLICATE KEY UPDATE token_sha256 = VALUES(token_sha256), created_at = VALUES(created_at)'
        )->execute([$req['module_id'], token_hash($token), utc_now()]);
        $pdo->prepare("UPDATE enroll_request SET state = 'consumed' WHERE id = ?")->execute([$req['id']]);
        $pdo->commit();
        return [200, ['status' => 'approved', 'moduleId' => $req['module_id'], 'token' => $token]];
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}

/** Efface ce que le serveur sait d'un module, hors la ligne module elle-meme. */
function enroll_purge_module_data(PDO $pdo, string $moduleId): void
{
    foreach (['module_message', 'command', 'module_config_backup', 'module_config'] as $table) {
        $pdo->prepare("DELETE FROM $table WHERE module_id = ?")->execute([$moduleId]);
    }
}

function enroll_claim_failures(int $userId, string $ip): array
{
    $stmt = db()->prepare(
        'SELECT SUM(user_id = ?) AS par_compte, SUM(ip = ?) AS par_ip FROM enroll_claim_attempt '
        . 'WHERE succeeded = 0 AND ts > (UTC_TIMESTAMP(3) - INTERVAL ? MINUTE)'
    );
    $stmt->execute([$userId, $ip, ENROLL_CLAIM_WINDOW_MIN]);
    $row = $stmt->fetch() ?: [];
    return [(int)($row['par_compte'] ?? 0), (int)($row['par_ip'] ?? 0)];
}

function enroll_record_claim(int $userId, string $ip, bool $ok): void
{
    db()->prepare('INSERT INTO enroll_claim_attempt (ts, user_id, ip, succeeded) VALUES (?, ?, ?, ?)')
        ->execute([utc_now(), $userId, $ip !== '' ? $ip : null, $ok ? 1 : 0]);
}

/**
 * Saisie du code par l'utilisateur connecte : rattache le module au compte.
 *
 * Un hw_id deja rattache a un autre compte vaut TRANSFERT (le code n'a pu
 * etre obtenu que par le module lui-meme, voir enroll_start) : les donnees
 * de l'ancien proprietaire sont purgees -- elles ne passent jamais au
 * nouveau -- et il est prevenu par mail.
 */
function enroll_claim(int $userId, string $rawCode, string $ip): array
{
    $reglages = enroll_settings();
    if ($reglages === null) {
        return ENROLL_NOT_CONFIGURED;
    }
    [$parCompte, $parIp] = enroll_claim_failures($userId, $ip);
    $max = $reglages['ENROLL_CLAIM_MAX_FAILURES'];
    if ($parCompte >= $max || $parIp >= 3 * $max) {
        return [429, ['detail' => 'trop de codes faux, reessayez dans ' . ENROLL_CLAIM_WINDOW_MIN . ' minutes']];
    }
    $code = enroll_normalize_user_code($rawCode);
    if ($code === null) {
        enroll_record_claim($userId, $ip, false);
        return [400, ['detail' => ENROLL_CLAIM_REFUSED]];
    }

    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare(
            "SELECT id, hw_id FROM enroll_request WHERE user_code_sha256 = ? AND state = 'pending' "
            . 'AND expires_at > UTC_TIMESTAMP(3) LIMIT 2 FOR UPDATE'
        );
        $stmt->execute([hash('sha256', $code)]);
        $reqs = $stmt->fetchAll();
        // Deux demandes vivantes sur le meme code (1 chance sur 2^40) :
        // ambigu, donc refuse plutot que de rattacher le mauvais boitier.
        if (count($reqs) !== 1) {
            $pdo->commit();
            enroll_record_claim($userId, $ip, false);
            return [400, ['detail' => ENROLL_CLAIM_REFUSED]];
        }
        $req = $reqs[0];
        $hwId = $req['hw_id'];

        $module = enroll_module_for_hw($hwId, true);
        $ancien = null;
        if ($module === null) {
            // Nouveau boitier : module_id = hw_id (D016 §4) ; le nom affiche
            // est un libelle que l'utilisateur choisit ensuite.
            $moduleId = $hwId;
            $pdo->prepare(
                'INSERT INTO module (module_id, hw_id, created_at) VALUES (?, ?, ?) '
                . 'ON DUPLICATE KEY UPDATE hw_id = COALESCE(hw_id, VALUES(hw_id))'
            )->execute([$moduleId, $hwId, utc_now()]);
        } else {
            $moduleId = $module['module_id'];
            // Proprietaire actuel, ou celui qui l'a detache en gardant ses
            // donnees : dans les deux cas, elles ne sont pas pour un autre.
            $precedent = $module['owner_user_id'] ?? $module['released_owner_user_id'];
            if ($precedent !== null && (int)$precedent !== $userId) {
                $ancien = (int)$precedent;
            }
        }

        if ($ancien !== null) {
            enroll_purge_module_data($pdo, $moduleId);
            $pdo->prepare('UPDATE module SET label = NULL WHERE module_id = ?')->execute([$moduleId]);
        }
        $pdo->prepare('UPDATE module SET owner_user_id = ?, released_owner_user_id = NULL, hw_id_conflict = NULL '
            . 'WHERE module_id = ?')->execute([$userId, $moduleId]);
        $pdo->prepare("UPDATE enroll_request SET state = 'approved', user_id = ?, module_id = ?, approved_at = ? WHERE id = ?")
            ->execute([$userId, $moduleId, utc_now(), $req['id']]);
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
    enroll_record_claim($userId, $ip, true);

    if ($ancien !== null) {
        $stmt = db()->prepare('SELECT email FROM app_user WHERE user_id = ?');
        $stmt->execute([$ancien]);
        $email = $stmt->fetchColumn();
        if (is_string($email)) {
            // Apres validation : un mail qui echoue ne doit pas defaire un
            // transfert legitime. L'echec est journalise par send_mail.
            send_mail($email, 'AquaLook : un de vos modules a change de proprietaire',
                "Le module " . $moduleId . " vient d'etre rattache a un autre compte depuis son ecran.\n\n"
                . "Il n'apparait plus dans votre espace, et ses donnees y ont ete effacees.\n"
                . "Si vous n'etes pas a l'origine de ce changement, repondez a ce message.\n",
                'notice', $ip);
        }
    }
    return [200, ['ok' => true, 'moduleId' => $moduleId, 'transfert' => $ancien !== null]];
}

/**
 * Desenrolement par le proprietaire : revoque le jeton et detache. Le
 * module recoit 401 au cycle suivant. $purge efface aussi l'historique, la
 * configuration et les sauvegardes conservees par le serveur.
 */
function enroll_release(string $moduleId, bool $purge): void
{
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $pdo->prepare('DELETE FROM module_token WHERE module_id = ?')->execute([$moduleId]);
        // Donnees purgees : plus rien a proteger, released_owner_user_id NULL.
        // Donnees gardees : on retient a qui elles sont (voir enroll_claim).
        $pdo->prepare('UPDATE module SET released_owner_user_id = '
            . ($purge ? 'NULL' : 'owner_user_id') . ', owner_user_id = NULL WHERE module_id = ?')
            ->execute([$moduleId]);
        if ($purge) {
            enroll_purge_module_data($pdo, $moduleId);
        }
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
}
