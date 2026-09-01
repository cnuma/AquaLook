<?php
/**
 * Chargeur .env minimal, sans dependance externe -- un hebergement mutualise
 * ne garantit pas toujours composer/vendor. Format simple : CLE=valeur, une
 * par ligne, # pour les commentaires.
 */

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
        $actuelle = getenv($key);
        if ($actuelle === false || $actuelle === '') {
            putenv("$key=$value");
            $_ENV[$key] = $value;
        }
    }
}
