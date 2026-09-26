#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager;

// Explications lisibles pour les codes courts places en tete de certaines
// lignes du journal technique (ex. "[ORCH-PREVIEW] zone=3 START pret=non").
//
// Contrainte d'origine (26 septembre 2026) : ne pas agrandir LOG_MSG_LEN
// (120 octets, EventLog.h) pour ne pas refaire tout le dimensionnement RAM
// du journal -- donc les lignes elles-memes restent courtes, techniques,
// et c'est ICI, dans un fichier de la carte SD consulte a la demande depuis
// /logs, que vit la phrase qui dit ce qu'il faut en penser/faire.
//
// Meme principe que ScriptMessageCatalogue (code -> phrase sur SD, jamais
// bloquant si la carte est absente), mais fichier et cle differents : la
// cle est ici un court code textuel ("ORCH-PREVIEW"), pas un entier, pour
// rester lisible seul dans une capture serie meme sans acces a la carte.
//
// Fichier : /logs/messages.tsv -- une entree par ligne :
//     <code><TAB><phrase>\n
// Hors de /www, donc un deploiement des ressources Web n'y touche pas.
//
// En cas de panne SD, de fichier absent ou de code non liste, phrase()
// echoue et l'appelant garde la ligne brute du journal -- jamais moins
// lisible qu'aujourd'hui par manque de repli.
namespace EventLogCatalogue {

constexpr uint8_t     MAX_ENTRIES = 32U;
constexpr uint8_t     MAX_CODE    = 20U;   // ex. "ORCH-PREVIEW" (12) + marge
constexpr uint8_t     MAX_PHRASE  = 200U;  // octets, UTF-8 -- carte SD, place dispo
constexpr const char* PATH        = "/logs/messages.tsv";

// A appeler une fois au demarrage, apres StorageManager::begin().
void begin(StorageManager* storage);

// Copie dans `out` (borne a `n`) la phrase associee a `code`. Rend false si
// storage non lie, SD indisponible, fichier absent, memoire insuffisante,
// code introuvable ou ligne illisible -- l'appelant garde alors la ligne
// brute du journal.
bool phrase(const char* code, char* out, size_t n);

// Remplit `out` : { entries:[{code,texte}...], max, lenMax }. Pour la route
// GET consultee une fois par chargement de /logs (pas a chaque poll).
// Rend false si storage non lie ou carte illisible ; le tableau `entries`
// est vide si le fichier n'existe pas encore.
bool load(JsonDocument& out);

// Ecrit `body` (octets TSV exacts, chaque ligne terminee par \n) de facon
// atomique -- fichier .tmp puis renommage. Pas d'editeur web pour ce
// catalogue (contrairement a ScriptMessageCatalogue) : sert au deploiement
// initial/mises a jour ponctuelles du contenu depuis un outil, pas a une
// saisie utilisateur. Rend false si carte non montee ou ecriture en echec ;
// dans ce cas le fichier en place reste intact.
bool store(const String& body);

}  // namespace EventLogCatalogue
