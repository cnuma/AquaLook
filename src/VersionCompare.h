#pragma once

#include <stdlib.h>

// Versions fonctionnelles "MAJEUR.MINEUR.CORRECTIF", suffixe eventuel
// ("-dev", "-beta", "+build") ignore pour la comparaison.
//
// Sert a l'anti-rejeu des mises a jour Web (P1.9 lot 2) : un manifeste
// signe reste valide pour toujours, et les anciennes versions restent en
// ligne ; seule la comparaison avec la version installee empeche de
// reinstaller une version anterieure.
namespace VersionCompare {

// Vrai si s est exactement trois nombres separes par des points, suivis de
// rien ou d'un suffixe. "depot direct", un SHA Git ou "" sont refuses : ce
// ne sont pas des versions comparables.
inline bool parse(const char* s, unsigned long out[3]) {
    if (!s) return false;
    for (int i = 0; i < 3; ++i) {
        if (*s < '0' || *s > '9') return false;
        char* end = nullptr;
        out[i] = strtoul(s, &end, 10);
        s = end;
        if (i < 2) {
            if (*s != '.') return false;
            ++s;
        }
    }
    return *s == '\0' || *s == '-' || *s == '+';
}

// <0 si a est anterieure a b, 0 si egales, >0 si a est plus recente.
inline int compare(const unsigned long a[3], const unsigned long b[3]) {
    for (int i = 0; i < 3; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

}  // namespace VersionCompare
