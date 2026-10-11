<?php
/**
 * Parametres du service modifiables depuis la console d'administration,
 * sans redeploiement ni edition du .env (demande du proprietaire, 9 oct.
 * 2026 : boite mail, serveurs et limites ne doivent etre ecrits en dur
 * nulle part).
 *
 * Ordre de lecture : table app_setting, puis .env, puis la valeur par
 * defaut du registre ci-dessous. Le .env reste donc un repli valable, et
 * effacer un parametre en base revient a ce repli.
 *
 * RESTENT DANS LE .env SEULEMENT : DB_* et ADMIN_TOKEN. Ce sont eux qui
 * permettent de lire la base et d'ouvrir la console ; les mettre en base
 * serait circulaire.
 *
 * Les parametres secrets (mot de passe SMTP) ne sont JAMAIS relus par
 * l'API : la console sait seulement s'ils sont definis. Depuis le 11 oct.
 * 2026 (TODO P0.4c) ils sont CHIFFRES en base avec la cle SETTINGS_KEY du
 * .env du serveur : une sauvegarde ou une fuite de la base seule ne donne
 * plus le mot de passe de la boite mail ; il faut aussi le .env. Une
 * ancienne valeur en clair reste lue, et se rechiffre d'elle-meme a la
 * premiere lecture des que la cle existe. Sans cle, un nouveau secret est
 * refuse plutot qu'ecrit en clair.
 *
 * SETTINGS_KEY rejoint donc DB_* et ADMIN_TOKEN dans le .env (une cle en
 * base a cote de ce qu'elle chiffre ne protegerait rien). La perdre rend le
 * secret illisible : il suffit alors de le ressaisir dans la console.
 */

declare(strict_types=1);

require_once __DIR__ . '/db.php';

/**
 * Registre : seuls ces noms sont acceptes en ecriture. Ajouter un
 * parametre = une ligne ici, aucune autre page a modifier (la console lit
 * ce registre par GET /admin/settings).
 */
const SETTINGS_REGISTRY = [
    'MAIL_SMTP_HOST' => ['type' => 'str', 'secret' => false, 'default' => '',
        'label' => 'Serveur SMTP sortant'],
    'MAIL_SMTP_PORT' => ['type' => 'int', 'secret' => false, 'default' => '465', 'min' => 1, 'max' => 65535,
        'label' => 'Port SMTP (TLS implicite)'],
    'MAIL_SMTP_USER' => ['type' => 'str', 'secret' => false, 'default' => '',
        'label' => 'Identifiant SMTP (adresse de la boite)'],
    'MAIL_SMTP_PASS' => ['type' => 'str', 'secret' => true, 'default' => '',
        'label' => 'Mot de passe SMTP'],
    'MAIL_FROM' => ['type' => 'email', 'secret' => false, 'default' => '',
        'label' => 'Adresse d\'expedition'],
    'MAIL_FROM_NAME' => ['type' => 'str', 'secret' => false, 'default' => 'AquaLook',
        'label' => 'Nom d\'expedition'],
    'MAIL_MAX_PER_RECIPIENT_HOUR' => ['type' => 'int', 'secret' => false, 'default' => '5', 'min' => 1, 'max' => 100,
        'label' => 'Mails par adresse et par heure'],
    'MAIL_MAX_PER_IP_HOUR' => ['type' => 'int', 'secret' => false, 'default' => '10', 'min' => 1, 'max' => 1000,
        'label' => 'Mails par adresse IP et par heure'],
    'MAIL_MAX_PER_DAY' => ['type' => 'int', 'secret' => false, 'default' => '200', 'min' => 1, 'max' => 10000,
        'label' => 'Mails par jour (tout le service)'],
    // Enrolement par code court (D016, lot D, enroll.php).
    'ENROLL_CODE_TTL_S' => ['type' => 'int', 'secret' => false, 'default' => '600', 'min' => 120, 'max' => 3600,
        'label' => 'Validite d\'un code de rattachement (s)'],
    'ENROLL_POLL_INTERVAL_S' => ['type' => 'int', 'secret' => false, 'default' => '5', 'min' => 2, 'max' => 60,
        'label' => 'Intervalle d\'interrogation du module pendant le rattachement (s)'],
    'ENROLL_MAX_PER_HW_HOUR' => ['type' => 'int', 'secret' => false, 'default' => '5', 'min' => 1, 'max' => 100,
        'label' => 'Demandes de code par module et par heure'],
    'ENROLL_MAX_PER_IP_HOUR' => ['type' => 'int', 'secret' => false, 'default' => '20', 'min' => 1, 'max' => 1000,
        'label' => 'Demandes de code par adresse IP et par heure'],
    'ENROLL_CLAIM_MAX_FAILURES' => ['type' => 'int', 'secret' => false, 'default' => '10', 'min' => 1, 'max' => 100,
        'label' => 'Codes faux par compte en 15 minutes'],
    // Comptes (D016, lot B, account.php). APP_BASE_URL sert a construire les
    // liens envoyes par mail : jamais l'en-tete Host de la requete, qu'un
    // attaquant choisit (voir account.php).
    'APP_BASE_URL' => ['type' => 'url', 'secret' => false, 'default' => '',
        'label' => 'Adresse publique du site, pour les liens des mails (https://...)'],
    'RESET_TOKEN_TTL_MIN' => ['type' => 'int', 'secret' => false, 'default' => '30', 'min' => 10, 'max' => 240,
        'label' => "Validite d'un lien mot de passe oublie (min)"],
    'INVITE_TOKEN_TTL_H' => ['type' => 'int', 'secret' => false, 'default' => '72', 'min' => 1, 'max' => 336,
        'label' => "Validite d'un lien d'invitation (h)"],
];

/** Valeurs en base, lues une fois par requete. Table absente = aucune. */
function settings_from_db(): array
{
    static $cache = null;
    if ($cache !== null) {
        return $cache;
    }
    try {
        $rows = db()->query('SELECT name, value FROM app_setting')->fetchAll();
        $cache = array_column($rows, 'value', 'name');
    } catch (PDOException $e) {
        // schema-v4 pas encore importe : on retombe sur le .env, comme
        // avant ce fichier. Une base injoignable echouera de toute facon
        // plus loin, avec son propre diagnostic.
        $cache = [];
    }
    return $cache;
}

// ── Chiffrement des secrets en base (P0.4c) ─────────────────────────────
//
// XChaCha20-Poly1305 (libsodium, standard depuis PHP 7.2) : nonce aleatoire
// de 24 octets, sans risque de repetition. Le nom du parametre est lie au
// chiffre (donnee associee) : recopier en base le chiffre d'un secret sous
// un autre nom ne le dechiffre pas. Format stocke :
//   enc1:<base64(nonce . chiffre)>
// La colonne fait 255 caracteres : d'ou SETTINGS_SECRET_MAX_LEN.
const SETTINGS_ENC_PREFIX = 'enc1:';
const SETTINGS_SECRET_MAX_LEN = 128;

/**
 * Cle de 32 octets lue dans SETTINGS_KEY (base64), ou null si absente,
 * mal formee ou libsodium indisponible. Generer avec :
 *   php -r "echo base64_encode(random_bytes(32));"
 */
function settings_key(): ?string
{
    if (!function_exists('sodium_crypto_aead_xchacha20poly1305_ietf_encrypt')) {
        return null;
    }
    $key = base64_decode(env_value('SETTINGS_KEY'), true);
    return ($key !== false && strlen($key) === SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_KEYBYTES) ? $key : null;
}

function settings_encrypt(string $name, string $plain, string $key): string
{
    $nonce = random_bytes(SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES);
    $box = sodium_crypto_aead_xchacha20poly1305_ietf_encrypt($plain, $name, $nonce, $key);
    return SETTINGS_ENC_PREFIX . base64_encode($nonce . $box);
}

/** Texte clair, ou null si la cle manque ou ne correspond pas. */
function settings_decrypt(string $name, string $stored): ?string
{
    $key = settings_key();
    $raw = base64_decode(substr($stored, strlen(SETTINGS_ENC_PREFIX)), true);
    $n = SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES;
    if ($key === null || $raw === false || strlen($raw) <= $n) {
        return null;
    }
    $plain = sodium_crypto_aead_xchacha20poly1305_ietf_decrypt(substr($raw, $n), $name, substr($raw, 0, $n), $key);
    return $plain === false ? null : $plain;
}

/** Etat d'un secret en base : 'chiffre', 'clair', 'illisible' ou null (absent). */
function settings_secret_state(string $name): ?string
{
    $stored = settings_from_db()[$name] ?? '';
    if ($stored === '') {
        return null;
    }
    if (!str_starts_with($stored, SETTINGS_ENC_PREFIX)) {
        return 'clair';
    }
    return settings_decrypt($name, $stored) === null ? 'illisible' : 'chiffre';
}

/**
 * Rechiffre un secret encore en clair (ecrit avant P0.4c), si la cle existe.
 * Ecriture conditionnelle (WHERE value = ancienne) : deux requetes
 * simultanees ne s'ecrasent pas. Un echec ne bloque rien, la valeur claire
 * reste lisible et le prochain passage reessaiera.
 */
function settings_migrate_plain(string $name, string $plain): void
{
    $key = settings_key();
    if ($key === null || strlen($plain) > SETTINGS_SECRET_MAX_LEN) {
        return;
    }
    try {
        db()->prepare('UPDATE app_setting SET value = ?, updated_at = ? WHERE name = ? AND value = ?')
            ->execute([settings_encrypt($name, $plain, $key), utc_now(), $name, $plain]);
    } catch (Throwable $e) {
        // sans consequence : voir ci-dessus
    }
}

/** Valeur effective et sa provenance ('base', 'env' ou 'defaut'). */
function setting_with_source(string $name): array
{
    $db = settings_from_db();
    if (isset($db[$name]) && $db[$name] !== '') {
        $value = $db[$name];
        if (!empty(SETTINGS_REGISTRY[$name]['secret'])) {
            if (str_starts_with($value, SETTINGS_ENC_PREFIX)) {
                // Illisible (cle perdue ou changee) : vide, et non le repli
                // .env -- la console le signale et invite a ressaisir.
                return [settings_decrypt($name, $value) ?? '', 'base'];
            }
            settings_migrate_plain($name, $value);
        }
        return [$value, 'base'];
    }
    $env = env_value($name);
    if ($env !== '') {
        return [$env, 'env'];
    }
    return [SETTINGS_REGISTRY[$name]['default'] ?? '', 'defaut'];
}

function setting(string $name): string
{
    return setting_with_source($name)[0];
}

function setting_int(string $name): int
{
    return (int)setting($name);
}

/** Vue pour la console : jamais la valeur d'un secret. */
function settings_list(): array
{
    $out = [];
    foreach (SETTINGS_REGISTRY as $name => $def) {
        [$value, $source] = setting_with_source($name);
        $out[] = [
            'name'    => $name,
            'label'   => $def['label'],
            'type'    => $def['type'],
            'secret'  => $def['secret'],
            'value'   => $def['secret'] ? null : $value,
            'defined' => $value !== '',
            'source'  => $source,
            // Secrets seulement : etat du stockage en base (null = pas en base).
            'stored'  => $def['secret'] ? settings_secret_state($name) : null,
        ];
    }
    return $out;
}

/** https obligatoire, sauf banc local : un lien de mot de passe en clair
 *  se lirait sur le reseau. Ni chemin de requete, ni parametre, ni fragment. */
function settings_url_ok(string $url): bool
{
    $p = parse_url($url);
    if ($p === false || !isset($p['scheme'], $p['host']) || isset($p['query']) || isset($p['fragment'])
        || isset($p['user'])) {
        return false;
    }
    if ($p['scheme'] === 'https') {
        return true;
    }
    return $p['scheme'] === 'http' && in_array($p['host'], ['localhost', '127.0.0.1'], true);
}

/**
 * Ecrit des parametres. null = effacer de la base (retour au .env ou au
 * defaut). Rend null si tout est valide et ecrit, sinon le message d'erreur
 * -- rien n'est ecrit si un seul parametre est invalide.
 */
function settings_save(array $values): ?string
{
    $ecrire = [];
    foreach ($values as $name => $value) {
        $def = SETTINGS_REGISTRY[$name] ?? null;
        if ($def === null) {
            return 'parametre inconnu : ' . $name;
        }
        if ($value === null) {
            $ecrire[$name] = null;
            continue;
        }
        if (!is_string($value) && !is_int($value)) {
            return 'valeur invalide : ' . $name;
        }
        $value = trim((string)$value);
        if (strlen($value) > 255 || preg_match('/[\r\n]/', $value)) {
            return 'valeur invalide : ' . $name;
        }
        if ($def['type'] === 'int' && $value !== '') {
            if (!ctype_digit($value) || (int)$value < $def['min'] || (int)$value > $def['max']) {
                return $name . ' doit etre un entier entre ' . $def['min'] . ' et ' . $def['max'];
            }
        }
        if ($def['type'] === 'url' && $value !== '' && !settings_url_ok($value)) {
            return $name . ' doit etre une adresse https:// (http:// seulement pour localhost)';
        }
        if ($def['type'] === 'email' && $value !== '' && !filter_var($value, FILTER_VALIDATE_EMAIL)) {
            return 'adresse invalide : ' . $name;
        }
        if ($def['secret'] && $value !== '') {
            if (strlen($value) > SETTINGS_SECRET_MAX_LEN) {
                return $name . ' : ' . SETTINGS_SECRET_MAX_LEN . ' caracteres au plus';
            }
            $key = settings_key();
            if ($key === null) {
                return 'SETTINGS_KEY absente ou invalide dans le .env du serveur : '
                    . 'secret non enregistre (il ne doit pas etre stocke en clair)';
            }
            $value = settings_encrypt($name, $value, $key);
        }
        $ecrire[$name] = $value === '' ? null : $value;
    }
    $pdo = db();
    $pdo->beginTransaction();
    try {
        $del = $pdo->prepare('DELETE FROM app_setting WHERE name = ?');
        $put = $pdo->prepare(
            'INSERT INTO app_setting (name, value, updated_at) VALUES (?, ?, ?) '
            . 'ON DUPLICATE KEY UPDATE value = VALUES(value), updated_at = VALUES(updated_at)'
        );
        foreach ($ecrire as $name => $value) {
            if ($value === null) {
                $del->execute([$name]);
            } else {
                $put->execute([$name, $value, utc_now()]);
            }
        }
        $pdo->commit();
    } catch (Throwable $e) {
        $pdo->rollBack();
        throw $e;
    }
    return null;
}
