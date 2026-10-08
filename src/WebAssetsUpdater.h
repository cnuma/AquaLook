#pragma once

#include <Arduino.h>
#include <functional>

class StorageManager;

// Etape 4 du plan "Mise a jour distante des ressources Web" (ROADMAP.md) :
// telecharger un fichier depuis une URL de release GitHub et verifier sa
// taille et son empreinte SHA-256, sans encore rien ecrire sur la carte SD
// (l'ecriture reelle, avec sa strategie de coherence en cas d'interruption,
// est l'etape 5, separee et non encore faite).
//
// Volontairement une classe distincte d'OtaDownloadTest, meme si la logique
// de telechargement HTTPS/redirection/SHA-256 se ressemble beaucoup : cette
// classe tourne en fonctionnement normal (WiFi, SD, Web deja actifs), pas
// dans la tache FreeRTOS isolee et minimale du mode maintenance OTA
// (MaintenanceSetupWrapper.cpp). C'est exactement la raison pour laquelle
// ROADMAP.md decrit un canal decouple : partager le code entre les deux
// recreerait un couplage que cette isolation existe justement pour eviter.
struct WebAssetVerifyResult {
    bool success = false;
    uint32_t downloadedSize = 0;
    uint32_t downloadDurationMs = 0;
    char calculatedSha256[65] = {0};
    char detail[40] = {0};
};

class WebAssetsUpdater {
public:
    // Destination des octets telecharges. Le telechargement, la verification
    // d'empreinte et la destination sont ainsi separes : la meme mecanique
    // sert a verifier sans ecrire, a remplir un tampon (manifeste) ou a ecrire
    // sur la carte, sans dupliquer la gestion HTTPS/redirections/SHA-256.
    // Retourner false interrompt le transfert (ecriture impossible).
    using Sink = std::function<bool(const uint8_t* data, size_t len)>;

    // Telecharge, verifie taille et SHA-256, et transmet les octets au sink au
    // fil de l'eau. Rien n'est conserve en memoire au-dela d'un bloc.
    static WebAssetVerifyResult downloadToSink(
        const char* url,
        uint32_t expectedSize,
        const char* expectedSha256Hex,
        const Sink& sink
    );

    // ── Detection d'une mise a jour disponible ────────────────────────────
    //
    // Le manifeste est recupere via l'URL "latest" de GitHub, qui resout
    // elle-meme la derniere release publiee — pas besoin d'interroger l'API ni
    // de connaitre le numero de version a l'avance. Meme mecanisme que l'OTA
    // firmware (OtaBuildIdentity::MANIFEST_PATH).
    // Source par defaut. Reste la release GitHub publiee : un module qui n'a
    // jamais rien configure se comporte exactement comme avant.
    static constexpr const char* DEFAULT_MANIFEST_URL =
        "https://github.com/cnuma/AquaLook/releases/latest/download/aqualook-web-manifest.json";

    // Source effective, poussee par ConfigManager au chargement de la
    // configuration. Le sens de la dependance est volontaire : la
    // configuration est l'autorite et alimente ce module, qui n'a ainsi
    // aucun lien vers elle.
    //
    // setManifestUrl() n'accepte que du https:// - le manifeste porte les
    // SHA-256 qui authentifient chaque fichier, donc un manifeste servi en
    // clair permettrait d'en substituer un autre, avec ses propres hashes.
    // La verification par hash validerait alors l'attaque au lieu de
    // l'empecher. Une URL refusee laisse la precedente en place.
    static void        setManifestUrl(const char* url);
    static const char* manifestUrl();

    // Taille maximale acceptee pour le manifeste. Le generateur impose deja
    // 8 Ko (tools/generate_web_manifest.py) ; cette borne protege le module
    // d'un fichier inattendu qui epuiserait sa memoire.
    static constexpr uint32_t MANIFEST_MAX_BYTES = 8192UL;

    struct CheckResult {
        bool ok = false;              // le manifeste a pu etre lu et analyse
        bool updateAvailable = false; // version publiee != version installee
        char availableVersion[24] = {0};
        char installedVersion[24] = {0};
        uint8_t fileCount = 0;
        char detail[40] = {0};
    };

    struct DeployResult {
        bool ok = false;
        uint8_t filesDeployed = 0;
        uint8_t fileCount = 0;
        char version[24] = {0};
        char detail[48] = {0};
    };

    // Deploiement complet : manifeste -> transit -> verification de chaque
    // fichier -> bascule. Concu pour s'executer en MODE MAINTENANCE, ou le
    // module dispose d'environ 245 Ko de tas et d'une tache dediee, au lieu
    // des ~32 Ko et de la pile partagee du fonctionnement normal — c'est ce
    // qui faisait echouer les tentatives precedentes (poignee de main TLS
    // impossible, puis debordement de pile de loopTask).
    static DeployResult deployFromManifest(StorageManager* storage);

    // Telecharge le manifeste, l'analyse et le compare a la version installee
    // (lue dans /www/assets-version.json). Ne modifie rien.
    static CheckResult checkForUpdate(StorageManager* storage);

    // Version des ressources Web deployees (/www/assets-version.json), la
    // meme lecture que checkForUpdate(). Chaine vide si SD absente, fichier
    // manquant ou illisible. Lecture seule ; sert a la page A propos du LCD.
    static void installedVersion(StorageManager* storage, char* out, size_t outSize);

    // url doit etre une URL https:// vers un hote GitHub autorise (release
    // asset). expectedSha256Hex : 64 caracteres hexadecimaux minuscules.
    // Le corps telecharge n'est jamais conserve, seulement haché au vol.
    static WebAssetVerifyResult verifyOnly(
        const char* url,
        uint32_t expectedSize,
        const char* expectedSha256Hex
    );
};
