<?php
/**
 * Envoi de mails (decision D016, lot A) : reinitialisation de mot de passe,
 * invitation, enrolement, notifications.
 *
 * POURQUOI UN CLIENT SMTP ECRIT ICI
 *
 * mail() ne dit rien de ce qui se passe : il rend true des que le message
 * est confie a sendmail, et la documentation AlwaysData se contredit sur la
 * necessite de s'authentifier depuis un site heberge. Le SMTP authentifie
 * sur la boite dediee (smtp-<compte>.alwaysdata.net, port 465, TLS
 * implicite) donne une reponse du serveur a chaque etape, donc une cause
 * lisible en cas d'echec. Pas de Composer sur ce serveur (voir env.php) :
 * le protocole utile tient en quelques commandes.
 *
 * POURQUOI LES LIMITES PASSENT AVANT L'ENVOI
 *
 * AlwaysData ne publie aucun plafond d'envoi, mais coupe les POST du site
 * et change le mot de passe de la boite quand il detecte un abus, puis peut
 * suspendre le compte en cas de recidive. Un formulaire "mot de passe
 * oublie" sans limite mettrait donc en danger tout le service, modules
 * compris. Chaque demande est comptee dans mail_log, envoyee ou non.
 *
 * Contenu en texte brut, enveloppe identique a l'en-tete From : c'est ce
 * que l'anti-spam Rspamd de l'hebergeur attend (score > 3 = bloque).
 *
 * Aucun secret ni parametre ecrit ici : serveur, boite, expediteur et
 * plafonds se reglent depuis la console d'administration (settings.php),
 * avec le .env comme repli.
 */

declare(strict_types=1);

require_once __DIR__ . '/settings.php';

// Les plafonds glissants (MAIL_MAX_*) sont des parametres, voir
// settings.php : un utilisateur demande un lien de reinitialisation une ou
// deux fois, pas dix -- les valeurs par defaut en partent.
const MAIL_SMTP_TIMEOUT_S = 10;
const MAIL_KINDS = ['test', 'reset', 'invite', 'enroll', 'notice'];

/** true si les parametres SMTP indispensables sont renseignes -- rien de plus. */
function mail_configured(): bool
{
    foreach (['MAIL_SMTP_HOST', 'MAIL_SMTP_USER', 'MAIL_SMTP_PASS', 'MAIL_FROM'] as $cle) {
        if (setting($cle) === '') {
            return false;
        }
    }
    return true;
}

/**
 * Empreinte de l'adresse, pour compter sans conserver les adresses en clair
 * dans un journal qui ne sert qu'a limiter le debit.
 */
function mail_recipient_hash(string $to): string
{
    return hash('sha256', strtolower(trim($to)));
}

/** Rend null si l'envoi est permis, sinon la limite atteinte. */
function mail_rate_limited(string $recipientHash, string $ip): ?string
{
    $stmt = db()->prepare(
        'SELECT '
        . ' SUM(recipient_sha256 = ? AND ts > (UTC_TIMESTAMP(3) - INTERVAL 1 HOUR)) AS par_adresse,'
        . ' SUM(ip = ? AND ts > (UTC_TIMESTAMP(3) - INTERVAL 1 HOUR)) AS par_ip,'
        . ' COUNT(*) AS par_jour'
        . ' FROM mail_log WHERE ts > (UTC_TIMESTAMP(3) - INTERVAL 1 DAY)'
    );
    $stmt->execute([$recipientHash, $ip]);
    $row = $stmt->fetch() ?: [];
    if ((int)($row['par_adresse'] ?? 0) >= setting_int('MAIL_MAX_PER_RECIPIENT_HOUR')) {
        return 'limite_adresse';
    }
    if ($ip !== '' && (int)($row['par_ip'] ?? 0) >= setting_int('MAIL_MAX_PER_IP_HOUR')) {
        return 'limite_ip';
    }
    if ((int)($row['par_jour'] ?? 0) >= setting_int('MAIL_MAX_PER_DAY')) {
        return 'limite_jour';
    }
    return null;
}

function mail_record(string $kind, string $recipientHash, string $ip, bool $ok, ?string $error): void
{
    $stmt = db()->prepare(
        'INSERT INTO mail_log (ts, kind, recipient_sha256, ip, ok, error) VALUES (?, ?, ?, ?, ?, ?)'
    );
    $stmt->execute([utc_now(), $kind, $recipientHash, $ip !== '' ? $ip : null, $ok ? 1 : 0, $error]);
}

/**
 * Point d'entree unique. Rend ['ok' => bool, 'error' => ?string] ou error
 * est une CATEGORIE (config, adresse, limite_*, connexion, tls, auth,
 * expediteur, destinataire, contenu, protocole), jamais un message du
 * serveur SMTP ni un identifiant : le resultat peut finir dans une reponse
 * HTTP.
 *
 * Les appelants exposes au public (mot de passe oublie) ne doivent PAS
 * relayer cette categorie a l'utilisateur : leur reponse reste identique
 * quel que soit le resultat, pour ne pas reveler si une adresse existe.
 */
function send_mail(string $to, string $subject, string $text, string $kind, string $ip): array
{
    if (!in_array($kind, MAIL_KINDS, true)) {
        throw new InvalidArgumentException('type de mail inconnu');
    }
    $to = trim($to);
    // CR/LF interdits : ils permettraient d'injecter des en-tetes ou des
    // commandes SMTP depuis un champ de formulaire.
    if (!filter_var($to, FILTER_VALIDATE_EMAIL) || strlen($to) > 190
        || preg_match('/[\r\n]/', $to . $subject)) {
        return ['ok' => false, 'error' => 'adresse'];
    }
    if (!mail_configured()) {
        return ['ok' => false, 'error' => 'config'];
    }
    $hash = mail_recipient_hash($to);
    $limite = mail_rate_limited($hash, $ip);
    if ($limite !== null) {
        // Compte aussi : sinon un attaquant insisterait sans que rien ne
        // l'enregistre une fois le plafond atteint.
        mail_record($kind, $hash, $ip, false, $limite);
        return ['ok' => false, 'error' => $limite];
    }
    $error = smtp_send($to, $subject, $text);
    mail_record($kind, $hash, $ip, $error === null, $error);
    if ($error !== null) {
        error_log('AquaLook mail: echec ' . $kind . ' (' . $error . ')');
    }
    return ['ok' => $error === null, 'error' => $error];
}

/** Lit une reponse SMTP complete (lignes "250-..." puis "250 ..."). */
function smtp_read($sock): string
{
    $reponse = '';
    while (($ligne = fgets($sock, 1024)) !== false) {
        $reponse .= $ligne;
        if (strlen($ligne) < 4 || $ligne[3] !== '-') {
            break;
        }
    }
    return $reponse;
}

/** Envoie une commande, rend true si le code de reponse est celui attendu. */
function smtp_step($sock, ?string $commande, string $codeAttendu): bool
{
    if ($commande !== null && fwrite($sock, $commande . "\r\n") === false) {
        return false;
    }
    return str_starts_with(smtp_read($sock), $codeAttendu);
}

function mail_encode_header(string $s): string
{
    return preg_match('/[^\x20-\x7e]/', $s) ? '=?UTF-8?B?' . base64_encode($s) . '?=' : $s;
}

/** Message RFC 5322 complet, texte brut en quoted-printable, fins de ligne CRLF. */
function mail_build(string $from, string $fromName, string $to, string $subject, string $text): string
{
    $domaine = substr(strrchr($from, '@') ?: '@localhost', 1);
    // CRLF AVANT l'encodage : quoted_printable_encode() ne garde que les
    // CRLF comme fins de ligne et code un LF seul en "=0A" -- le corps
    // arriverait sur une seule ligne. Constate au banc le 9 oct. 2026.
    $texte = str_replace("\n", "\r\n", str_replace(["\r\n", "\r"], "\n", $text));
    $corps = quoted_printable_encode($texte);
    $entetes = [
        'Date: ' . gmdate('D, d M Y H:i:s') . ' +0000',
        'From: ' . mail_encode_header($fromName) . ' <' . $from . '>',
        'To: <' . $to . '>',
        'Subject: ' . mail_encode_header($subject),
        'Message-ID: <' . bin2hex(random_bytes(16)) . '@' . $domaine . '>',
        'MIME-Version: 1.0',
        'Content-Type: text/plain; charset=UTF-8',
        'Content-Transfer-Encoding: quoted-printable',
    ];
    // Transparence SMTP (RFC 5321 4.5.2) : une ligne commencant par un point
    // est doublee, sinon une ligne "." seule terminerait le message.
    $corps = preg_replace('/^\./m', '..', $corps);
    return implode("\r\n", $entetes) . "\r\n\r\n" . $corps;
}

/**
 * Transport SMTP, TLS implicite, AUTH LOGIN. Rend null si le serveur a
 * accepte le message, sinon une categorie d'erreur. Le certificat du
 * serveur est verifie (jamais de verify_peer=false) ; MAIL_SMTP_CAFILE
 * (.env seulement, volontairement hors de la console) n'existe que pour un
 * banc local a certificat auto-signe.
 */
function smtp_send(string $to, string $subject, string $text): ?string
{
    $host = setting('MAIL_SMTP_HOST');
    $port = setting_int('MAIL_SMTP_PORT');
    $user = setting('MAIL_SMTP_USER');
    $pass = setting('MAIL_SMTP_PASS');
    $from = setting('MAIL_FROM');
    $fromName = setting('MAIL_FROM_NAME');

    // Sans l'extension, le transport ssl:// n'existe pas et l'echec
    // ressemblerait a une panne reseau ; c'est un defaut d'installation.
    if (!extension_loaded('openssl')) {
        return 'config';
    }
    $ssl = ['verify_peer' => true, 'verify_peer_name' => true, 'peer_name' => $host];
    $cafile = env_value('MAIL_SMTP_CAFILE');
    if ($cafile !== '') {
        $ssl['cafile'] = $cafile;
    }
    $errno = 0;
    $errstr = '';
    $sock = @stream_socket_client(
        'ssl://' . $host . ':' . $port, $errno, $errstr, MAIL_SMTP_TIMEOUT_S,
        STREAM_CLIENT_CONNECT, stream_context_create(['ssl' => $ssl])
    );
    if ($sock === false) {
        // errno 0 a la connexion = echec de la negociation TLS (certificat).
        return $errno === 0 ? 'tls' : 'connexion';
    }
    stream_set_timeout($sock, MAIL_SMTP_TIMEOUT_S);
    try {
        $ehlo = 'EHLO ' . (gethostname() ?: 'aqualook');
        if (!smtp_step($sock, null, '220') || !smtp_step($sock, $ehlo, '250')) {
            return 'protocole';
        }
        if (!smtp_step($sock, 'AUTH LOGIN', '334')
            || !smtp_step($sock, base64_encode($user), '334')
            || !smtp_step($sock, base64_encode($pass), '235')) {
            return 'auth';
        }
        if (!smtp_step($sock, 'MAIL FROM:<' . $from . '>', '250')) {
            return 'expediteur';
        }
        if (!smtp_step($sock, 'RCPT TO:<' . $to . '>', '25')) {
            return 'destinataire';
        }
        if (!smtp_step($sock, 'DATA', '354')) {
            return 'protocole';
        }
        // Refus apres DATA : c'est ici que l'anti-spam se prononce.
        if (!smtp_step($sock, mail_build($from, $fromName, $to, $subject, $text) . "\r\n.", '250')) {
            return 'contenu';
        }
        smtp_step($sock, 'QUIT', '221');
        return null;
    } finally {
        fclose($sock);
    }
}
