<?php
/**
 * Purge periodique de l'historique -- destine a une tache planifiee
 * AlwaysData (CLI), jamais a une requete HTTP.
 *
 * Comble le manque signale dans README.md ("Purge de l'historique") :
 * module_message grossit sans limite pour diag/status/state/event, faute de
 * dedoublonnage possible -- la telemetrie varie a chaque envoi, contrairement
 * a config (deja deduplique a l'insertion, voir db.php/latest_config).
 * login_attempt et app_session accumulent de la meme facon, sans jamais etre
 * relues au-dela de leur fenetre utile.
 *
 * Volontairement un script CLI independant, et non un evenement MySQL : meme
 * raisonnement que schema-v3-sauvegarde.sql pour module_config_backup --
 * l'hebergement mutualise ne garantit pas que le planificateur d'evenements
 * MySQL soit actif, alors qu'une tache planifiee AlwaysData est un vrai cron
 * au niveau de l'hebergement, independant de la base.
 *
 * command et module_config_backup ne sont PAS touches ici : le premier a une
 * valeur de tracabilite exigee par SYSTEM_ARCHITECTURE.md Sec.7, le second est
 * deja elague a l'insertion (prune_config_backups, BACKUP_KEEP dans auth.php).
 *
 * Aucune erreur n'est avalee : une exception remonte telle quelle et sort en
 * echec (code non nul), pour qu'AlwaysData signale l'incident au lieu qu'une
 * purge cesse de tourner en silence.
 */

declare(strict_types=1);

if (PHP_SAPI !== 'cli') {
    // Ceinture en plus du blocage .htaccess : ce fichier existe sur disque,
    // donc la reecriture vers index.php ne s'applique pas (meme situation que
    // .env/schema.sql). Un acces HTTP direct ne doit jamais lancer de purge.
    http_response_code(403);
    exit('Accessible uniquement en ligne de commande.');
}

require_once __DIR__ . '/db.php';

// Types sans dedoublonnage possible -- voir schema.sql. 'config' est exclu :
// il n'est deja journalise que sur changement (meme principe que
// module_config_backup) et garde une valeur d'audit a faible volume.
const PURGEABLE_MESSAGE_TYPES = ['diag', 'status', 'state', 'event'];
const MESSAGE_RETENTION_DAYS       = 90;
const LOGIN_ATTEMPT_RETENTION_DAYS = 30;

function purge_old_messages(PDO $pdo): int
{
    $placeholders = implode(',', array_fill(0, count(PURGEABLE_MESSAGE_TYPES), '?'));
    $stmt = $pdo->prepare(
        "DELETE FROM module_message WHERE msg_type IN ($placeholders) "
        . 'AND ts < (UTC_TIMESTAMP(3) - INTERVAL ? DAY)'
    );
    $stmt->execute([...PURGEABLE_MESSAGE_TYPES, MESSAGE_RETENTION_DAYS]);
    return $stmt->rowCount();
}

function purge_old_login_attempts(PDO $pdo): int
{
    $stmt = $pdo->prepare(
        'DELETE FROM login_attempt WHERE ts < (UTC_TIMESTAMP(3) - INTERVAL ? DAY)'
    );
    $stmt->execute([LOGIN_ATTEMPT_RETENTION_DAYS]);
    return $stmt->rowCount();
}

function purge_expired_sessions(PDO $pdo): int
{
    // Pas de fenetre de tolerance : une session expiree n'a plus aucun usage,
    // contrairement aux deux purges ci-dessus qui gardent volontairement une
    // marge (audit, diagnostic recent).
    $stmt = $pdo->prepare('DELETE FROM app_session WHERE expires_at < UTC_TIMESTAMP(3)');
    $stmt->execute();
    return $stmt->rowCount();
}

// Le journal des mails ne sert qu'aux plafonds (une heure, un jour) et au
// diagnostic recent. Table absente (schema-v4 pas importe) : rien a purger,
// et ce n'est pas une raison de faire echouer les autres purges.
const MAIL_LOG_RETENTION_DAYS = 30;

function purge_old_mail_log(PDO $pdo): int
{
    try {
        $stmt = $pdo->prepare('DELETE FROM mail_log WHERE ts < (UTC_TIMESTAMP(3) - INTERVAL ? DAY)');
        $stmt->execute([MAIL_LOG_RETENTION_DAYS]);
        return $stmt->rowCount();
    } catch (PDOException $e) {
        if ($e->getCode() === '42S02') {
            return 0;
        }
        throw $e;
    }
}

$pdo = db();
$messages = purge_old_messages($pdo);
$attempts = purge_old_login_attempts($pdo);
$sessions = purge_expired_sessions($pdo);
$mails = purge_old_mail_log($pdo);

fwrite(STDOUT, sprintf(
    "[%s] purge : %d message(s), %d tentative(s) de connexion, %d session(s) expiree(s), %d mail(s) journalise(s)\n",
    utc_now(), $messages, $attempts, $sessions, $mails
));
