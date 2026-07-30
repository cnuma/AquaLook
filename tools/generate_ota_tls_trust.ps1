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

        $pemText = [System.IO.File]::ReadAllText($pemPath).Replace("`r`n", "`n").Trim()
        if (-not $pemText.StartsWith("-----BEGIN CERTIFICATE-----") -or
            -not $pemText.EndsWith("-----END CERTIFICATE-----")) {
            throw "Format PEM invalide pour $($entry.Name)."
        }

        $base64 = ($pemText -split "`n" | Where-Object { $_ -notmatch '^-----' }) -join ''
        $der = [Convert]::FromBase64String($base64)
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
// Sources officielles DigiCert et empreintes SHA-256 controlees avant generation.
namespace OtaTlsTrust {
inline constexpr char ROOT_CA_PEM[] = R"AQLCERT(
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
