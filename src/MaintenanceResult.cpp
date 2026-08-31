#include "MaintenanceResult.h"

#include <Preferences.h>
#include <ctime>
#include <cstring>

namespace {
constexpr char NVS_NAMESPACE[] = "aq_maint_res";
constexpr char NVS_KEY[] = "blob";
constexpr uint32_t NVS_MAGIC = 0x53455252UL; // "RRES" lu petit-boutiste
// Schema 3 (31 aout 2026) : ajout de recordedEpoch, la date reelle de
// l enregistrement. Meme mecanique de migration que ci-dessous : le blob
// schema 2 differe en taille, il est rejete et l on repart d un resultat
// vide. On perd le dernier resultat une fois, au premier demarrage sur ce
// firmware -- sans consequence, une nouvelle verification le reconstruit.
//
// Schema 2 (18 aout 2026) : ajout des champs webAssets* (canal ressources Web
// dans la verification periodique). Un blob schema 1 differe en taille, donc
// loadRaw() le rejette et repart d'un MaintenanceResult{} par defaut -- migration
// deja geree par le controle payloadSize/CRC existant, aucun code de migration
// explicite necessaire pour ce blob transitoire.
constexpr uint16_t NVS_SCHEMA = 3U;

// Bloc unique, a l'image de PersistedConfig dans ConfigManager : une seule
// ecriture NVS au lieu d'une quinzaine de cles separees. Chaque cle NVS a un
// cout fixe minimal independant de sa taille ; regrouper les champs en un
// seul blob reduit fortement l'empreinte totale et le nombre d'ecritures
// flash par sauvegarde.
struct PersistedMaintenanceResult {
    uint32_t magic;
    uint16_t schema;
    uint16_t payloadSize;
    MaintenanceResult data;
    uint32_t crc32;
};

uint32_t crc32Bytes(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}

void copyText(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0U) return;
    std::strncpy(destination, source ? source : "", destinationSize - 1U);
    destination[destinationSize - 1U] = '\0';
}

// Charge le bloc precedent sans passer par l'API publique load(), afin de
// distinguer explicitement "aucun bloc NVS" de "bloc invalide" au besoin.
MaintenanceResult loadRaw(Preferences& preferences) {
    MaintenanceResult empty;
    const size_t len = preferences.getBytesLength(NVS_KEY);
    if (len != sizeof(PersistedMaintenanceResult)) return empty;

    // Alloue sur le tas et non sur la pile : PersistedMaintenanceResult pese
    // environ 760 octets, et ce code est appele depuis des taches a pile
    // etroite. Le 17 aout 2026, l'enchainement load() puis save() depuis la
    // tache de notification (4 Ko) a fait deborder le canari de pile. Meme
    // precaution que ConfigManager::save(), pour la meme raison.
    PersistedMaintenanceResult* blob =
        static_cast<PersistedMaintenanceResult*>(malloc(sizeof(PersistedMaintenanceResult)));
    if (blob == nullptr) return empty;

    const size_t read = preferences.getBytes(NVS_KEY, blob, sizeof(*blob));
    if (read != sizeof(*blob) ||
        blob->magic != NVS_MAGIC ||
        blob->schema != NVS_SCHEMA ||
        blob->payloadSize != sizeof(*blob)) {
        free(blob);
        return empty;
    }
    if (crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                    offsetof(PersistedMaintenanceResult, crc32)) != blob->crc32) {
        free(blob);
        return empty;
    }
    const MaintenanceResult data = blob->data;
    free(blob);
    return data;
}
}

MaintenanceResult MaintenanceResultStore::load() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, true)) return MaintenanceResult{};
    const MaintenanceResult result = loadRaw(preferences);
    preferences.end();
    return result;
}

bool MaintenanceResultStore::save(const MaintenanceResult& result) {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;

    const MaintenanceResult previous = loadRaw(preferences);

    const bool isVersionCheck = strcmp(result.command, "check_version") == 0;
    const bool isDownloadTest = strcmp(result.command, "download_update_test") == 0;
    const bool isStageTest = strcmp(result.command, "stage_update_test") == 0;
    const bool successfulVersionCheck = isVersionCheck && result.success;
    const bool successfulInstall = strcmp(result.command, "install_update") == 0 && result.success;
    const bool successfulWebAssetsDeploy =
        strcmp(result.command, "web_assets_update") == 0 && result.success;

    const bool previousUpdateAvailable = previous.updateAvailable;
    const bool previousNotificationPending = previous.notificationPending;
    const bool explicitNotificationAck = previousUpdateAvailable && previousNotificationPending &&
        result.updateAvailable && !result.notificationPending && result.availableVersion[0] != '\0' &&
        strcmp(previous.availableVersion, result.availableVersion) == 0;

    MaintenanceResult merged = result;

    // Horodatage pose ICI et non sur les dix sites d'appel de save() : un seul
    // endroit, donc aucun risque qu'un chemin d'echec oublie de dater son
    // resultat -- et c'est precisement un echec qu'on cherche a dater.
    //
    // Seuil a 2020 : une horloge non reglee demarre en 1970, et afficher
    // "echec du 1er janvier 1970" serait pire que ne rien afficher. Zero
    // signifie explicitement "date inconnue", l'interface le dit ainsi.
    {
        const time_t now = time(nullptr);
        merged.recordedEpoch =
            (now > static_cast<time_t>(1577836800)) ? static_cast<uint32_t>(now) : 0U;
    }

    if (!successfulVersionCheck) {
        // Un INSTALL_UPDATE reussi consomme la mise a jour en attente : la
        // notification et le drapeau "mise a jour disponible" ne doivent pas
        // survivre a la bascule, sinon le nouveau firmware demarre en
        // pretendant a tort qu'une mise a jour vers lui-meme reste a faire.
        merged.updateAvailable = successfulInstall ? false : previousUpdateAvailable;
        merged.notificationPending = successfulInstall
            ? false
            : (explicitNotificationAck ? false : previousNotificationPending);
        merged.manifestSize = previous.manifestSize;
        merged.firmwareSize = previous.firmwareSize;
        copyText(merged.availableVersion, sizeof(merged.availableVersion), previous.availableVersion);
        copyText(merged.channel, sizeof(merged.channel), previous.channel);
        copyText(merged.firmwareUrl, sizeof(merged.firmwareUrl), previous.firmwareUrl);

        // installedVersion / target / environment / board decrivent le
        // PROGRAMME QUI TOURNE, pas le catalogue interroge. Ils sont donc
        // toujours connus, meme quand la verification echoue -- les remplacer
        // par l'historique les effacait des qu'un echec survenait sans
        // antecedent, et l'interface affichait "installee : inconnue" pour une
        // version que le module connait par construction. Constate le 31 aout
        // 2026, apres la remise a zero du bloc par le passage au schema 3 :
        // l'aide invitait a lire la "Cible attendue" d'un champ vide.
        //
        // On garde donc la valeur fraiche, et on ne retombe sur l'ancienne que
        // si elle manque.
        if (result.installedVersion[0] == '\0')
            copyText(merged.installedVersion, sizeof(merged.installedVersion), previous.installedVersion);
        if (result.target[0] == '\0')
            copyText(merged.target, sizeof(merged.target), previous.target);
        if (result.environment[0] == '\0')
            copyText(merged.environment, sizeof(merged.environment), previous.environment);
        if (result.board[0] == '\0')
            copyText(merged.board, sizeof(merged.board), previous.board);
        copyText(merged.sha256, sizeof(merged.sha256), previous.sha256);
        // Canal ressources Web : un WEB_ASSETS_UPDATE reussi consomme la mise a
        // jour en attente ; toute autre commande preserve le dernier resultat
        // connu du canal Web.
        //
        // MAIS seulement si ce CHECK_VERSION n'a rien rapporte du canal Web.
        //
        // Les deux canaux sont independants par conception : CHECK_VERSION
        // interroge le firmware PUIS les ressources Web, et l'echec du premier
        // n'empeche pas le second (MaintenanceBoot.cpp). Ecraser
        // inconditionnellement effacait pourtant le resultat frais du canal Web
        // des que le canal firmware echouait -- ce qui est l'etat PERMANENT
        // d'une carte dont la cible OTA vaut "unsupported". Le canal Web ne
        // pouvait alors plus jamais signaler une mise a jour, sur cette carte
        // comme sur toute autre apres un simple echec reseau du canal firmware.
        // Constate le 1er septembre 2026 : trois champs webAssets* vides apres
        // une verification ou le canal Web avait pourtant repondu.
        //
        // La version disponible non vide est le temoin fiable que le canal Web
        // a bien tourne : checkForUpdate() ne la renseigne qu'apres avoir lu et
        // valide le manifeste.
        const bool webChannelReported = result.webAssetsAvailableVersion[0] != '\0';
        if (!webChannelReported) {
            merged.webAssetsUpdateAvailable =
                successfulWebAssetsDeploy ? false : previous.webAssetsUpdateAvailable;
            merged.webAssetsNotificationPending =
                successfulWebAssetsDeploy ? false : previous.webAssetsNotificationPending;
            copyText(merged.webAssetsInstalledVersion, sizeof(merged.webAssetsInstalledVersion),
                     previous.webAssetsInstalledVersion);
            copyText(merged.webAssetsAvailableVersion, sizeof(merged.webAssetsAvailableVersion),
                     successfulWebAssetsDeploy ? "" : previous.webAssetsAvailableVersion);
        } else if (successfulWebAssetsDeploy) {
            // Un deploiement reussi consomme la mise a jour, meme si le canal
            // vient aussi de se prononcer.
            merged.webAssetsUpdateAvailable = false;
            merged.webAssetsNotificationPending = false;
        }
        if (!isDownloadTest && !isStageTest) {
            merged.downloadedSize = previous.downloadedSize;
            merged.downloadDurationMs = previous.downloadDurationMs;
            copyText(merged.calculatedSha256, sizeof(merged.calculatedSha256), previous.calculatedSha256);
        }
    } else {
        merged.downloadedSize = 0U;
        merged.downloadDurationMs = 0U;
        merged.calculatedSha256[0] = '\0';
        if (result.updateAvailable && previousUpdateAvailable &&
            strcmp(previous.availableVersion, result.availableVersion) == 0 &&
            !previousNotificationPending) {
            merged.notificationPending = false;
        }
        // Meme deduplication pour le canal Web : une verification qui retrouve
        // la meme version deja notifiee ne doit pas remettre la notification en
        // attente chaque jour.
        if (result.webAssetsUpdateAvailable && previous.webAssetsUpdateAvailable &&
            strcmp(previous.webAssetsAvailableVersion, result.webAssetsAvailableVersion) == 0 &&
            !previous.webAssetsNotificationPending) {
            merged.webAssetsNotificationPending = false;
        }
    }

    // Sur le tas, pour la meme raison que dans loadRaw() : ce bloc de ~760
    // octets s'ajoutait a 'previous' et 'merged' deja sur la pile, soit plus
    // de 2 Ko pour la seule fonction save().
    PersistedMaintenanceResult* blob =
        static_cast<PersistedMaintenanceResult*>(malloc(sizeof(PersistedMaintenanceResult)));
    if (blob == nullptr) {
        preferences.end();
        return false;
    }
    memset(blob, 0, sizeof(*blob));
    blob->magic = NVS_MAGIC;
    blob->schema = NVS_SCHEMA;
    blob->payloadSize = sizeof(*blob);
    blob->data = merged;
    blob->crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                             offsetof(PersistedMaintenanceResult, crc32));

    const size_t written = preferences.putBytes(NVS_KEY, blob, sizeof(*blob));
    preferences.end();
    free(blob);
    return written == sizeof(PersistedMaintenanceResult);
}

bool MaintenanceResultStore::stampDateIfMissing() {
    // Le mode maintenance n'a pas d'horloge fiable : NTP n'y tourne pas, et
    // l'heure systeme ne survit pas au redemarrage qui y fait entrer. Un
    // resultat ecrit la-bas repart donc systematiquement avec recordedEpoch a
    // zero -- constate le 31 aout 2026, la page affichait "date inconnue" pour
    // une verification faite trente secondes plus tot.
    //
    // On le date donc au premier passage en mode normal ou l'heure est connue,
    // soit quelques dizaines de secondes apres l'operation. La date est donc
    // approchee par exces, jamais inventee : si l'horloge n'est toujours pas
    // reglee, on ne pose rien et l'interface continue de dire qu'elle
    // l'ignore.
    const time_t now = time(nullptr);
    if (now <= static_cast<time_t>(1577836800)) return false;

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;

    PersistedMaintenanceResult* blob =
        static_cast<PersistedMaintenanceResult*>(malloc(sizeof(PersistedMaintenanceResult)));
    if (blob == nullptr) { preferences.end(); return false; }

    const size_t read = preferences.getBytes(NVS_KEY, blob, sizeof(*blob));
    const bool usable = read == sizeof(*blob) &&
                        blob->magic == NVS_MAGIC &&
                        blob->schema == NVS_SCHEMA &&
                        blob->payloadSize == sizeof(*blob) &&
                        crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                                   offsetof(PersistedMaintenanceResult, crc32)) == blob->crc32;
    // Rien a dater : pas de bloc, bloc illisible, ou date deja posee. Le cas
    // "deja posee" est le plus frequent - cette fonction est appelee a chaque
    // demarrage, et ne doit ecrire qu'une seule fois par operation.
    if (!usable || !blob->data.valid || blob->data.recordedEpoch != 0U) {
        free(blob);
        preferences.end();
        return false;
    }

    blob->data.recordedEpoch = static_cast<uint32_t>(now);
    blob->crc32 = crc32Bytes(reinterpret_cast<const uint8_t*>(blob),
                             offsetof(PersistedMaintenanceResult, crc32));
    const size_t written = preferences.putBytes(NVS_KEY, blob, sizeof(*blob));
    preferences.end();
    free(blob);
    return written == sizeof(PersistedMaintenanceResult);
}

bool MaintenanceResultStore::clear() {
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;
    const bool ok = preferences.clear();
    preferences.end();
    return ok;
}
