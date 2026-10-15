#pragma once

// CSP des pages du module, source unique : pages integrees au firmware
// (P3.13, WebManager) et pages de la SD (P3.19, SdStaticHandler). La meme
// que celle du service en ligne (.htaccess) ; csp_banc_pages.py en verifie
// le coeur (script-src 'self'). Aucune page n'a plus de script inline ; les
// <style> et attributs style= le restent, d'ou style-src 'unsafe-inline'.
namespace AquaLook {
constexpr const char* PAGE_CSP =
    "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data:; connect-src 'self'; base-uri 'none'; "
    "form-action 'self'; frame-ancestors 'none'; object-src 'none'";
}  // namespace AquaLook
