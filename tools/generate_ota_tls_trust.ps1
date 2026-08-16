param(
    [string]$OutputPath = "src/OtaTlsTrust.h"
)

$ErrorActionPreference = "Stop"

$certificates = @(
    @{
        Name = "DigiCert Global Root CA"
        Url = "https://cacerts.digicert.com/DigiCertGlobalRootCA.crt.pem"
        Sha256 = "4348A0E9444C78CB265E058D5E8944B4D84F9662BD26DB257F8934A443C70161"
    },
    @{
        Name = "DigiCert Global Root G2"
        Url = "https://cacerts.digicert.com/DigiCertGlobalRootG2.crt.pem"
        Sha256 = "CB3CCBB76031E5E0138F8DD39A23F9DE47FFC35E43C1144CEA27D46A5AB1CB5F"
    },
    @{
        # Ancre de confiance reelle de github.com et api.github.com au
        # 2026-08-13 (chaine Sectigo Public Server Authentication CA DV E36
        # -> ... -> USERTrust ECC, verifiee sur matiere via openssl s_client).
        Name = "USERTrust ECC Certification Authority"
        Url = "http://crt.sectigo.com/USERTrustECCCertificationAuthority.crt"
        Sha256 = "4FF460D54B9C86DABFBCFC5712E0400D2BED3FBC4D4FBDAA86E06ADCD2A9AD7A"
    },
    @{
        # Ancre de confiance reelle de objects.githubusercontent.com et
        # release-assets.githubusercontent.com au 2026-08-13 (chaine Let's
        # Encrypt YR1 -> Root YR -> ISRG Root X1, verifiee sur meme base).
        Name = "ISRG Root X1"
        Url = "https://letsencrypt.org/certs/isrgrootx1.pem"
        Sha256 = "96BCEC06264976F37460779ACF28C5A7CFE8A3C0AAE11A8FFCEE05C0BDDF08C6"
    },
    @{
        # Ajoutee le 16 aout 2026 : github.com envoie la chaine leaf ->
        # "Sectigo Public Server Authentication CA DV E36" -> "...Root E46"
        # (elle-meme signee par USERTrust ECC, deja dans ce bundle). Verifiee
        # valide via openssl (openssl verify avec le bundle existant comme
        # CAfile) mais mbedTLS/ESP32 echoue en pratique sur ce module
        # (X509 - Certificate verification failed, -9984) — vraisemblablement
        # une limitation de construction de chaine sur 4 certificats. Ajouter
        # Root E46 comme ancre directement fiable raccourcit le chemin d'un
        # cran et evite d'avoir a s'appuyer sur cette construction. Empreinte
        # verifiee par deux sources independantes : telechargement direct
        # (URL ci-dessous) et premiere entree du paquet AIA "CA Issuers" de
        # l'intermediaire recu en direct de github.com (memes octets).
        Name = "Sectigo Public Server Authentication Root E46"
        Url = "http://crt.sectigo.com/SectigoPublicServerAuthenticationRootE46.crt"
        Sha256 = "C90F26F0FB1B4018B22227519B5CA2B53E2CA5B3BE5CF18EFE1BEF47380C5383"
    }
)

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$outputFile = Join-Path $repoRoot $OutputPath
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("aqualook-ota-ca-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    $pemBlocks = New-Object System.Collections.Generic.List[string]

    foreach ($entry in $certificates) {
        $pemPath = Join-Path $tempRoot (($entry.Name -replace '[^A-Za-z0-9]+', '_') + ".pem")
        Invoke-WebRequest -Uri $entry.Url -OutFile $pemPath -UseBasicParsing

        # Certaines autorites (ex. le depot historique Sectigo/USERTrust)
        # servent le certificat racine en DER binaire plutot qu'en PEM. On
        # detecte le format recu et on normalise vers du PEM dans les deux cas.
        $rawBytes = [System.IO.File]::ReadAllBytes($pemPath)
        $asText = [System.Text.Encoding]::ASCII.GetString($rawBytes)
        if ($asText.TrimStart().StartsWith("-----BEGIN CERTIFICATE-----")) {
            $pemText = $asText.Replace("`r`n", "`n").Trim()
            if (-not $pemText.EndsWith("-----END CERTIFICATE-----")) {
                throw "Format PEM invalide pour $($entry.Name)."
            }
            $base64 = ($pemText -split "`n" | Where-Object { $_ -notmatch '^-----' }) -join ''
            $der = [Convert]::FromBase64String($base64)
        } else {
            $der = $rawBytes
            $base64Body = [Convert]::ToBase64String($der, [Base64FormattingOptions]::InsertLineBreaks).Replace("`r`n", "`n")
            $pemText = "-----BEGIN CERTIFICATE-----`n$base64Body`n-----END CERTIFICATE-----"
        }

        $certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($der)
        $fingerprint = $certificate.GetCertHashString([System.Security.Cryptography.HashAlgorithmName]::SHA256)

        if ($fingerprint -ne $entry.Sha256) {
            throw "Empreinte SHA-256 incorrecte pour $($entry.Name): $fingerprint"
        }

        Write-Host ("OK {0} SHA-256={1}" -f $entry.Name, $fingerprint)
        $pemBlocks.Add($pemText + "`n")
    }

    $combinedPem = ($pemBlocks -join "")
    $header = @"
#pragma once

#include <WiFiClientSecure.h>

// Fichier genere par tools/generate_ota_tls_trust.ps1.
// Sources officielles (DigiCert, Sectigo/USERTrust, ISRG/Let's Encrypt) et
// empreintes SHA-256 controlees avant generation.
namespace OtaTlsTrust {
// static (et non inline) : le standard C++ actif pour ce projet ne supporte
// pas les variables inline (C++17). constexpr implique deja une liaison
// interne par unite de compilation, sans duplication ni erreur d'edition
// de liens.
static constexpr char ROOT_CA_PEM[] = R"AQLCERT(
$combinedPem)AQLCERT";

inline void configure(WiFiClientSecure& client) {
    client.setCACert(ROOT_CA_PEM);
}
}
"@

    $outputDirectory = Split-Path -Parent $outputFile
    if (-not (Test-Path $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }

    [System.IO.File]::WriteAllText(
        $outputFile,
        $header.Replace("`r`n", "`n"),
        [System.Text.UTF8Encoding]::new($false)
    )

    Write-Host "Header genere: $outputFile"
}
finally {
    Remove-Item -Recurse -Force $tempRoot -ErrorAction SilentlyContinue
}
