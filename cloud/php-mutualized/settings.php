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
 * Les parametres secrets (mot de passe SMTP) sont stockes en clair dans la
 * base -- il faut bien les presenter au serveur SMTP -- mais ne sont JAMAIS
 * relus par l'API : la console sait seulement s'ils sont definis. Une
 * sauvegarde de la base contient donc ce mot de passe ; c'est le prix de la
 * modification sans redeploiement, et le .env reste disponible pour qui
 * prefere l'eviter.
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

/** Valeur effective et sa provenance ('base', 'env' ou 'defaut'). */
function setting_with_source(string $name): array
{
    $db = settings_from_db();
    if (isset($db[$name]) && $db[$name] !== '') {
        return [$db[$name], 'base'];
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
        ];
    }
    return $out;
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
        if ($def['type'] === 'email' && $value !== '' && !filter_var($value, FILTER_VALIDATE_EMAIL)) {
            return 'adresse invalide : ' . $name;
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
