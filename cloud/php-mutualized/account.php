<?php
/**
 * Mot de passe oublie, changement de mot de passe et invitation (decision
 * D016, lot B ; docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md Sec.6).
 *
 * Un lien envoye par mail porte un jeton aleatoire de 256 bits, a usage
 * unique, dont seule l'empreinte est en base (schema-v7-liens-compte.sql).
 * Reinitialisation et invitation sont le meme geste -- poser le mot de passe
 * d'un compte -- avec deux durees de vie et deux textes.
 *
 * Le jeton voyage dans le FRAGMENT de l'URL (app.html#reset=...) : le
 * navigateur ne l'envoie jamais au serveur en GET, il n'apparait donc ni
 * dans les journaux d'acces ni dans un en-tete Referer. Seul le POST de
 * app.html le transmet, dans le corps.
 *
 * L'adresse de base des liens est un PARAMETRE (APP_BASE_URL, console), et
 * jamais l'en-tete Host de la requete : un attaquant pourrait sinon faire
 * envoyer a sa victime un vrai mail AquaLook pointant vers son propre site
 * (empoisonnement d'en-tete Host), et y recolter le jeton.
 */

declare(strict_types=1);

require_once __DIR__ . '/auth.php';
require_once __DIR__ . '/mail.php';

const ACCOUNT_PURPOSES = ['reset', 'invite'];
// bcrypt ignore tout au-dela de 72 octets ; le plafond evite surtout qu'un
// corps de requete demesure occupe password_hash().
const MAX_PASSWORD_LENGTH = 256;

/** Adresse publique du site, sans barre finale, ou null si non reglee. */
function app_base_url(): ?string
{
    $url = rtrim(setting('APP_BASE_URL'), '/');
    return $url !== '' ? $url : null;
}

/** Message d'erreur si le mot de passe est refuse, sinon null. */
function password_problem(string $password): ?string
{
    if (strlen($password) < MIN_PASSWORD_LENGTH) {
        return 'mot de passe trop court, ' . MIN_PASSWORD_LENGTH . ' caracteres minimum';
    }
    if (strlen($password) > MAX_PASSWORD_LENGTH) {
        return 'mot de passe trop long';
    }
    return null;
}

/**
 * Emet un lien pour ce compte et rend [jeton en clair, expiration UTC].
 * Les liens encore valables du meme compte sont annules : seul le dernier
 * mail recu fonctionne, ce qui evite qu'un vieux mail oublie dans une boite
 * reste une porte ouverte.
 */
function account_token_issue(int $userId, string $purpose, string $ip): array
{
    $ttlSec = $purpose === 'invite'
        ? setting_int('INVITE_TOKEN_TTL_H') * 3600
        : setting_int('RESET_TOKEN_TTL_MIN') * 60;
    $pdo = db();
    $pdo->prepare('UPDATE account_token SET used_at = ? WHERE user_id = ? AND used_at IS NULL')
        ->execute([utc_now(), $userId]);
    $token = bin2hex(random_bytes(32));
    $expires = (new DateTime('now', new DateTimeZone('UTC')))
        ->modify('+' . $ttlSec . ' seconds')->format('Y-m-d H:i:s.v');
    $pdo->prepare(
        'INSERT INTO account_token (token_sha256, user_id, purpose, created_at, expires_at, ip) '
        . 'VALUES (?, ?, ?, ?, ?, ?)'
    )->execute([hash('sha256', $token), $userId, $purpose, utc_now(), $expires, $ip !== '' ? $ip : null]);
    return [$token, $expires];
}

function account_link(string $purpose, string $token): string
{
    return app_base_url() . '/app.html#' . $purpose . '=' . $token;
}

/** Ferme les sessions du compte, sauf celle dont l'empreinte est donnee. */
function close_user_sessions(int $userId, ?string $keepTokenSha256 = null): void
{
    if ($keepTokenSha256 === null) {
        db()->prepare('DELETE FROM app_session WHERE user_id = ?')->execute([$userId]);
    } else {
        db()->prepare('DELETE FROM app_session WHERE user_id = ? AND token_sha256 <> ?')
            ->execute([$userId, $keepTokenSha256]);
    }
}

function set_user_password(int $userId, string $password): void
{
    db()->prepare('UPDATE app_user SET password_hash = ? WHERE user_id = ?')
        ->execute([password_hash($password, PASSWORD_DEFAULT), $userId]);
}

/** Mail de notification apres un changement de mot de passe. Echec ignore. */
function account_notice_password_changed(string $email, string $ip): void
{
    $base = app_base_url();
    send_mail(
        $email,
        'AquaLook : mot de passe modifié',
        "Bonjour,\n\n"
        . 'Le mot de passe de votre compte AquaLook (' . $email . ') a été modifié le '
        . gmdate('d/m/Y à H:i') . " UTC. Les autres sessions ouvertes ont été fermées.\n\n"
        . "Si vous n'êtes pas à l'origine de ce changement, utilisez « Mot de passe oublié »"
        . ($base !== null ? ' sur ' . $base . '/app.html' : '')
        . " et prévenez l'administrateur du service.\n",
        'notice',
        $ip
    );
}

/**
 * Mot de passe oublie. Rend un TRAVAIL A FAIRE APRES LA REPONSE (ou null) :
 * l'envoi SMTP prend plusieurs secondes, et une reponse plus lente pour une
 * adresse connue suffirait a savoir quels comptes existent. L'appelant
 * repond d'abord, identiquement dans tous les cas, puis execute ce travail.
 */
function account_forgot(string $email, string $ip): ?callable
{
    $email = strtolower(trim($email));
    if (!filter_var($email, FILTER_VALIDATE_EMAIL) || strlen($email) > 190) {
        return null;
    }
    $stmt = db()->prepare('SELECT user_id FROM app_user WHERE email = ?');
    $stmt->execute([$email]);
    $userId = $stmt->fetchColumn();
    if ($userId === false) {
        // Compte dans mail_log comme une demande refusee : les plafonds par
        // IP limitent ainsi aussi la recherche d'adresses, sans rien envoyer.
        $hash = mail_recipient_hash($email);
        if (mail_rate_limited($hash, $ip) === null) {
            mail_record('reset', $hash, $ip, false, 'inconnu');
        }
        return null;
    }
    // Plafond verifie AVANT d'emettre un jeton : au-dela, aucun mail ne
    // partira de toute facon, et l'ancien lien doit rester valable.
    $limite = mail_rate_limited(mail_recipient_hash($email), $ip);
    if ($limite !== null) {
        mail_record('reset', mail_recipient_hash($email), $ip, false, $limite);
        return null;
    }
    [$token] = account_token_issue((int)$userId, 'reset', $ip);
    $minutes = setting_int('RESET_TOKEN_TTL_MIN');
    $lien = account_link('reset', $token);
    return static function () use ($email, $ip, $lien, $minutes): void {
        send_mail(
            $email,
            'AquaLook : réinitialiser votre mot de passe',
            "Bonjour,\n\n"
            . 'Une réinitialisation du mot de passe de votre compte AquaLook (' . $email . ") a été demandée.\n\n"
            . 'Pour choisir un nouveau mot de passe, ouvrez ce lien (valable ' . $minutes
            . " minutes, utilisable une seule fois) :\n\n" . $lien . "\n\n"
            . "Si vous n'êtes pas à l'origine de cette demande, ignorez ce message : "
            . "votre mot de passe actuel reste valable.\n",
            'reset',
            $ip
        );
    };
}

/**
 * Consomme un lien et pose le mot de passe. Rend [statut HTTP, corps,
 * user_id ou null]. Un lien inconnu, expire ou deja utilise est
 * distingue : le jeton fait 256 bits, la difference n'aide personne a le
 * deviner, et elle aide l'utilisateur a comprendre quoi faire.
 */
function account_token_consume(string $token, string $password): array
{
    if (!preg_match('/^[0-9a-f]{64}$/', $token)) {
        return [400, ['detail' => 'lien invalide'], null];
    }
    $probleme = password_problem($password);
    if ($probleme !== null) {
        return [400, ['detail' => $probleme], null];
    }
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $stmt = $pdo->prepare(
            'SELECT t.id, t.user_id, t.purpose, t.used_at, (t.expires_at <= UTC_TIMESTAMP(3)) AS perime, u.email '
            . 'FROM account_token t JOIN app_user u ON u.user_id = t.user_id '
            . 'WHERE t.token_sha256 = ? FOR UPDATE'
        );
        $stmt->execute([hash('sha256', $token)]);
        $row = $stmt->fetch();
        if (!$row) {
            $pdo->rollBack();
            return [400, ['detail' => 'lien invalide'], null];
        }
        if ($row['used_at'] !== null) {
            $pdo->rollBack();
            return [410, ['detail' => 'ce lien a déjà servi ou a été remplacé par un plus récent'], null];
        }
        if ((int)$row['perime'] === 1) {
            $pdo->rollBack();
            return [410, ['detail' => 'ce lien a expiré, demandez-en un nouveau'], null];
        }
        $userId = (int)$row['user_id'];
        set_user_password($userId, $password);
        $pdo->prepare('UPDATE account_token SET used_at = ? WHERE user_id = ? AND used_at IS NULL')
            ->execute([utc_now(), $userId]);
        close_user_sessions($userId);
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
    return [200, ['ok' => true, 'purpose' => $row['purpose'], 'email' => $row['email']], $userId];
}

/**
 * Invitation par l'administrateur : cree le compte s'il n'existe pas (mot
 * de passe aleatoire jamais communique, donc inutilisable), emet un lien et
 * l'envoie. Le lien est AUSSI rendu a l'administrateur, pour qu'il puisse
 * le transmettre autrement si le mail n'arrive pas (indesirables).
 */
function account_invite(string $email, ?string $label, string $ip): array
{
    $email = strtolower(trim($email));
    if (!filter_var($email, FILTER_VALIDATE_EMAIL) || strlen($email) > 190) {
        return [400, ['detail' => 'adresse electronique invalide']];
    }
    if ($label !== null && strlen($label) > 120) {
        return [400, ['detail' => 'libelle invalide']];
    }
    if (app_base_url() === null) {
        return [503, ['detail' => 'APP_BASE_URL non regle dans la console', 'error' => 'not_configured']];
    }
    $stmt = db()->prepare('SELECT user_id FROM app_user WHERE email = ?');
    $stmt->execute([$email]);
    $userId = $stmt->fetchColumn();
    $existant = $userId !== false;
    if (!$existant) {
        $userId = create_user($email, bin2hex(random_bytes(32)), $label !== '' ? $label : null);
    }
    [$token, $expires] = account_token_issue((int)$userId, 'invite', $ip);
    $heures = setting_int('INVITE_TOKEN_TTL_H');
    $lien = account_link('invite', $token);
    $r = send_mail(
        $email,
        'AquaLook : votre espace en ligne',
        "Bonjour,\n\n"
        . 'Un compte AquaLook a été ouvert pour vous (' . $email . ").\n\n"
        . 'Pour choisir votre mot de passe et accéder à votre espace, ouvrez ce lien (valable '
        . $heures . " heures, utilisable une seule fois) :\n\n" . $lien . "\n\n"
        . "Vous pourrez ensuite y rattacher votre module avec le code affiché sur son écran "
        . "(ADMIN > En ligne > Rattacher).\n",
        'invite',
        $ip
    );
    return [200, [
        'userId'    => (int)$userId,
        'email'     => $email,
        'existing'  => $existant,
        'sent'      => $r['ok'],
        'error'     => $r['error'],
        'link'      => $lien,
        'expiresAt' => $expires,
    ]];
}

/**
 * Changement de mot de passe, session ouverte : exige le mot de passe
 * actuel (une session volee ne suffit pas a prendre le compte), compte les
 * echecs comme une connexion, ferme les AUTRES sessions.
 */
function account_change_password(array $user, string $current, string $password, string $ip): array
{
    [$parEmail, $parIp] = recent_failures($user['email'], $ip);
    if ($parEmail >= RATE_MAX_EMAIL || $parIp >= RATE_MAX_IP) {
        return [429, ['detail' => 'trop de tentatives, reessayez dans ' . RATE_WINDOW_MIN . ' minutes']];
    }
    if (verify_credentials($user['email'], $current) !== $user['userId']) {
        record_attempt($user['email'], $ip, false);
        return [403, ['detail' => 'mot de passe actuel incorrect']];
    }
    $probleme = password_problem($password);
    if ($probleme !== null) {
        return [400, ['detail' => $probleme]];
    }
    set_user_password($user['userId'], $password);
    $cookie = $_COOKIE[SESSION_COOKIE] ?? '';
    close_user_sessions($user['userId'], is_string($cookie) ? hash('sha256', $cookie) : null);
    db()->prepare('UPDATE account_token SET used_at = ? WHERE user_id = ? AND used_at IS NULL')
        ->execute([utc_now(), $user['userId']]);
    return [200, ['ok' => true]];
}

/**
 * Envoie la reponse JSON au client PUIS execute $apres (un envoi SMTP).
 * fastcgi_finish_request() ferme la connexion sous PHP-FPM ; ailleurs,
 * Content-Length et Connection: close laissent le client conclure des la
 * reponse recue. A verifier en production : les deux durees de
 * /app/password/forgot (adresse connue ou non) doivent etre proches.
 */
function respond_then(int $status, array $body, ?callable $apres): never
{
    http_response_code($status);
    $json = json_encode($body, JSON_UNESCAPED_UNICODE);
    if ($apres === null) {
        echo $json;
        exit;
    }
    ignore_user_abort(true);
    if (function_exists('fastcgi_finish_request')) {
        echo $json;
        fastcgi_finish_request();
    } else {
        // Repli hors PHP-FPM : longueur annoncee et connexion fermee, le
        // client a sa reponse complete avant l'envoi SMTP.
        header('Content-Length: ' . strlen($json));
        header('Connection: close');
        echo $json;
        while (ob_get_level() > 0) {
            ob_end_flush();
        }
        flush();
    }
    try {
        $apres();
    } catch (Throwable $e) {
        error_log('AquaLook compte: travail differe en echec (' . get_class($e) . ')');
    }
    exit;
}
