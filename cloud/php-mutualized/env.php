<?php
/**
 * Chargeur .env minimal, sans dependance externe -- un hebergement mutualise
 * ne garantit pas toujours composer/vendor. Format simple : CLE=valeur, une
 * par ligne, # pour les commentaires.
 */

/**
 * Valeur de configuration, cherchee aux trois endroits possibles.
 *
 * Les hebergeurs n'exposent pas les variables d'environnement au meme
 * endroit : getenv() sous mod_php, $_SERVER sous PHP-FPM (le pool les passe
 * en fastcgi_param), $_ENV seulement si variables_order contient E -- ce qui
 * n'est pas le reglage par defaut de PHP.
 *
 * Ne consulter que getenv() faisait passer pour "non configuree" une
 * variable pourtant correctement definie dans le panneau de l'hebergeur, et
 * le serveur repondait 503 sans moyen de comprendre pourquoi. Constate en
 * production le 1er septembre 2026.
 *
 * Rend toujours une chaine : "" signifie absente, et une variable declaree
 * sans valeur est traitee comme absente -- c'est ce que l'appelant veut
 * savoir.
 */
function env_value(string $key): string
{
    $v = getenv($key);
    if (is_string($v) && $v !== '') {
        return $v;
    }
    foreach ([$_SERVER, $_ENV] as $source) {
        if (isset($source[$key]) && is_string($source[$key]) && $source[$key] !== '') {
            return $source[$key];
        }
    }
    return '';
}

function load_env(string $path): void
{
    if (!is_readable($path)) {
        return;
    }
    foreach (file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
        $line = trim($line);
        if ($line === '' || str_starts_with($line, '#')) {
            continue;
        }
        [$key, $value] = array_pad(explode('=', $line, 2), 2, '');
        $key = trim($key);
        $value = trim($value);
        if ($key === '') {
            continue;
        }
        // Une variable d'environnement VIDE ne doit pas masquer une valeur
        // reelle du fichier.
        //
        // getenv() rend false quand la variable n'existe pas, mais "" quand
        // elle existe sans valeur -- ce qui arrive des qu'on la declare dans
        // le panneau d'un hebergeur sans la renseigner, ou qu'on la vide en
        // pensant la supprimer. L'ancien test ne comparait qu'a false : la
        // variable vide gagnait alors sur le fichier, et le serveur repondait
        // "ADMIN_TOKEN non configure" avec un .env parfaitement correct a cote.
        // Constate en production le 1er septembre 2026.
        // env_value() couvre les trois sources : une variable definie
        // dans le panneau de l'hebergeur ne doit pas etre ecrasee par le
        // fichier, ou qu'elle soit exposee.
        if (env_value($key) === '') {
            putenv("$key=$value");
            $_ENV[$key] = $value;
        }
    }
}
