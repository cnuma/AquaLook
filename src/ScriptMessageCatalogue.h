#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager;

// Bibliotheque de phrases pour « notifier <code> » et « message <code> ».
//
// CACHE EN RAM (8 oct. 2026) -- le fichier est lu une fois au demarrage et
// relu apres chaque store() reussi (editeur local ou config.apply). phrase()
// ne lit donc jamais la carte : un script qui ecrit au journal ne doit pas
// attendre une SD lente ou en panne. Tampon d'au plus ~4,5 Ko, en PSRAM
// quand elle existe.
//
// Le script ne transporte qu'un code sur 16 bits, inerte : n'importe quelle
// valeur est sans danger, le module la revalide sans effort. Le texte
// lisible vit ici, dans un fichier de la carte SD, et n'est resolu qu'au
// moment d'envoyer la notification. Cote utilisateur, sur le telephone, on
// lit la vraie phrase ; dans le bytecode, il n'y a qu'un pointeur
// referentiel.
//
// Fichier : /scripts/messages.tsv -- une entree par ligne :
//     <code><TAB><phrase>\n
// Hors de /www, donc un deploiement des ressources Web n'y touche pas ;
// openWrite() cree /scripts/ si besoin.
//
// En cas de panne SD, de fichier absent ou de code non liste, phrase()
// echoue et l'appelant retombe sur le code nu -- jamais d'echec du script
// pour une phrase manquante. Une panne SD porte par ailleurs sa propre
// notification : les deux messages ensemble disent quoi.
namespace ScriptMessageCatalogue {

constexpr uint8_t     MAX_ENTRIES = 48U;
constexpr uint8_t     MAX_PHRASE  = 80U;   // octets, UTF-8
constexpr const char* PATH        = "/scripts/messages.tsv";

// A appeler une fois au demarrage, apres StorageManager::begin() : charge
// le cache.
void begin(StorageManager* storage);

// Copie dans `out` (borne a `n`) la phrase associee a `code`, depuis le
// cache. Rend false si aucun cache (SD absente au demarrage et toujours
// absente), code introuvable ou ligne illisible -- l'appelant utilise alors
// le code nu.
bool phrase(uint16_t code, char* out, size_t n);

// Remplit `out` : { entries:[{code,texte}...], max, lenMax }. Pour la route
// GET et l'editeur. Rend false si storage non lie ou carte illisible ; le
// tableau `entries` est vide si le fichier n'existe pas encore.
bool load(JsonDocument& out);

// Ecrit `body` (deja valide en amont : octets TSV exacts, chaque ligne
// terminee par \n) de facon atomique -- fichier .tmp puis renommage -- puis
// recharge le cache. Rend false si carte non montee ou ecriture en echec ;
// dans ce cas le fichier en place et le cache restent intacts.
bool store(const String& body);

}  // namespace ScriptMessageCatalogue
