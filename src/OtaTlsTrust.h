#pragma once

#include <WiFiClientSecure.h>

// Declaration seule. Le paquet de certificats vit dans OtaTlsTrust.cpp, en un
// seul exemplaire et prive a cette unite -- voir la note qui y explique
// pourquoi. Cet en-tete est ecrit a la main et stable ; c'est le .cpp que
// tools/generate_ota_tls_trust.ps1 regenere.
namespace OtaTlsTrust {
void configure(WiFiClientSecure& client);
}
