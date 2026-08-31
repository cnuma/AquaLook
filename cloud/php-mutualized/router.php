<?php
/**
 * Routeur pour le serveur de developpement integre de PHP.
 *
 * Reproduit exactement ce que fait .htaccess sur l'hebergement : un fichier
 * qui existe sur disque est servi tel quel, tout le reste part dans
 * index.php. Sans lui, `php -S ... index.php` envoie TOUTES les requetes au
 * routeur JSON, y compris admin.html - la console est alors inaccessible en
 * local alors qu'elle fonctionne en production, ce qui est la pire forme de
 * divergence entre les deux environnements.
 *
 *   php -S 127.0.0.1:8001 router.php
 */

declare(strict_types=1);

$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH) ?: '/';
$file = __DIR__ . $path;

// Meme protection que le <FilesMatch> du .htaccess : le serveur de
// developpement ne le lit pas, et sans ce garde-fou .env serait servi en
// clair en local. Un secret qui fuit sur le poste de developpement est un
// secret qui a fuite.
if (preg_match('#(^|/)(\.env|schema\.sql|router\.php)$#', $path)) {
    http_response_code(403);
    exit;
}

if ($path !== '/' && is_file($file)) {
    return false;   // laisse le serveur integre servir le fichier statique
}

require __DIR__ . '/index.php';
