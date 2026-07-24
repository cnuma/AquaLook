#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>

namespace {

const char* chipModelName(esp_chip_model_t model) {
  switch (model) {
    case CHIP_ESP32: return "ESP32";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
    case CHIP_ESP32C2: return "ESP32-C2";
    case CHIP_ESP32C6: return "ESP32-C6";
    case CHIP_ESP32H2: return "ESP32-H2";
    default: return "ESP32 inconnu / non repertorie";
  }
}

void printBytes(const char* label, uint64_t value) {
  Serial.printf("%-31s : %llu octets (%.2f Mo)\n",
                label,
                static_cast<unsigned long long>(value),
                static_cast<double>(value) / (1024.0 * 1024.0));
}

void printSeparator() {
  Serial.println("------------------------------------------------------------");
}

void testPsramAllocation() {
  constexpr size_t testSize = 256 * 1024;

  if (!psramFound()) {
    Serial.println("Test allocation PSRAM          : NON EFFECTUE (PSRAM absente)");
    return;
  }

  void* block = heap_caps_malloc(testSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (block == nullptr) {
    Serial.printf("Test allocation PSRAM          : ECHEC (%u octets)\n",
                  static_cast<unsigned>(testSize));
    return;
  }

  memset(block, 0xA5, testSize);
  const uint8_t* bytes = static_cast<const uint8_t*>(block);
  bool valid = true;
  for (size_t i = 0; i < testSize; i += 4096) {
    if (bytes[i] != 0xA5) {
      valid = false;
      break;
    }
  }

  Serial.printf("Test allocation PSRAM          : %s (%u octets)\n",
                valid ? "OK" : "ECHEC CONTENU",
                static_cast<unsigned>(testSize));
  heap_caps_free(block);
}

void printDiagnostic() {
  esp_chip_info_t chipInfo{};
  esp_chip_info(&chipInfo);

  uint32_t flashSize = 0;
  const esp_err_t flashResult = esp_flash_get_size(nullptr, &flashSize);

  printSeparator();
  Serial.println("CYD CARD DIAGNOSTICS - IDENTIFICATION MATERIELLE");
  printSeparator();
  Serial.printf("Modele SoC                     : %s\n", chipModelName(chipInfo.model));
  Serial.printf("Modele Arduino                 : %s\n", ESP.getChipModel());
  Serial.printf("Revision puce                  : %u\n", chipInfo.revision);
  Serial.printf("Nombre de coeurs               : %u\n", chipInfo.cores);
  Serial.printf("Frequence CPU                  : %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Version SDK                    : %s\n", ESP.getSdkVersion());
  Serial.printf("Adresse MAC eFuse              : %012llX\n",
                static_cast<unsigned long long>(ESP.getEfuseMac()));

  printSeparator();
  Serial.println("MEMOIRE FLASH");
  if (flashResult == ESP_OK) {
    printBytes("Taille flash detectee", flashSize);
  } else {
    Serial.printf("Taille flash detectee          : erreur ESP-IDF %d\n", flashResult);
  }
  printBytes("Taille flash Arduino", ESP.getFlashChipSize());
  Serial.printf("Frequence flash                : %u MHz\n", ESP.getFlashChipSpeed() / 1000000U);
  Serial.printf("Mode flash                     : %u\n", static_cast<unsigned>(ESP.getFlashChipMode()));
  printBytes("Taille sketch", ESP.getSketchSize());
  printBytes("Espace sketch libre", ESP.getFreeSketchSpace());

  printSeparator();
  Serial.println("RAM INTERNE");
  printBytes("Heap total", ESP.getHeapSize());
  printBytes("Heap libre", ESP.getFreeHeap());
  printBytes("Heap minimum observe", ESP.getMinFreeHeap());
  printBytes("Plus grand bloc interne", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

  printSeparator();
  Serial.println("PSRAM");
  Serial.printf("Support compile BOARD_HAS_PSRAM: %s\n",
#ifdef BOARD_HAS_PSRAM
                "OUI"
#else
                "NON"
#endif
  );

  const bool present = psramFound();
  Serial.printf("PSRAM detectee au runtime      : %s\n", present ? "OUI" : "NON");
  printBytes("PSRAM totale", ESP.getPsramSize());
  printBytes("PSRAM libre", ESP.getFreePsram());
  printBytes("PSRAM minimum libre", ESP.getMinFreePsram());
  printBytes("Plus grand bloc PSRAM", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  testPsramAllocation();

  printSeparator();
  Serial.println("INTERPRETATION");
  if (present && ESP.getPsramSize() > 0) {
    Serial.println("RESULTAT : cette carte expose une PSRAM utilisable.");
  } else {
    Serial.println("RESULTAT : aucune PSRAM utilisable n'a ete detectee.");
  }
  Serial.println("NOTE     : le logiciel identifie la puce et ses memoires,");
  Serial.println("           mais pas de facon certaine la reference commerciale");
  Serial.println("           exacte du PCB CYD, qui ne possede pas d'identifiant standard.");
  printSeparator();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  printDiagnostic();
}

void loop() {
  delay(10000);
}
