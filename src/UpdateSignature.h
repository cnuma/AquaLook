#pragma once

#include <stddef.h>
#include <stdint.h>

// Signature des manifestes de mise a jour (D018) : ECDSA P-256 / SHA-256.
//
// Le manifeste porte la taille et le SHA-256 de chaque fichier ; le signer
// authentifie donc tout le paquet. La confiance ne repose plus sur
// l'hebergeur (ni sur son TLS, ni sur l'acces FTP) mais sur une cle privee
// gardee hors ligne par le proprietaire, dont seule la cle PUBLIQUE est
// integree au firmware (UpdateSignatureKeys.h, genere par
// tools/update_signing.py). Changer d'hebergeur ne change rien a la
// securite.
//
// Fichier de signature (texte, une ligne) : "aqualook-sig-v1 <id> <base64>",
// calcule sur les octets EXACTS du manifeste. Format et regle identiques a
// tools/update_signing.py (verifier_octets), qui sert de reference.
namespace UpdateSignature {

enum class Result : uint8_t {
    OK,
    UNREADABLE,    // format inconnu, base64 invalide, signature trop longue
    UNKNOWN_KEY,   // cle non integree a ce firmware (ou revoquee)
    INVALID,       // signature fausse : manifeste modifie ou contrefait
    INTERNAL,      // erreur mbedTLS (cle integree illisible...)
};

// Taille maximale acceptee pour le fichier .sig (une ligne d'environ 120
// octets ; la borne protege d'un fichier inattendu).
constexpr size_t SIG_FILE_MAX_BYTES = 512U;

// keyIdOut (17 octets au moins, peut etre nul) recoit l'identifiant lu.
Result verify(const uint8_t* data, size_t dataLen,
              const char* sigText, size_t sigLen,
              char* keyIdOut, size_t keyIdOutSize);

// Code court pour les journaux et le resultat de maintenance (aide :
// data/ota-help.json).
const char* code(Result r);

}  // namespace UpdateSignature
