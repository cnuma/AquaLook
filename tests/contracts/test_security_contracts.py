from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class SecurityContracts(unittest.TestCase):
    """Contrats statiques exécutables sans carte ESP32.

    Les expectedFailure représentent des risques confirmés et empêchent leur
    disparition silencieuse du suivi. Ils doivent être convertis en tests
    ordinaires dès que le firmware est corrigé.
    """

    @unittest.expectedFailure
    def test_wifi_password_is_never_logged_in_cleartext(self) -> None:
        web = source("src/WebManager.cpp")
        self.assertNotRegex(web, r"PWD='%s'")
        self.assertNotRegex(web, r"Serial\.printf\([^;]*\bpwd\b")

    @unittest.expectedFailure
    def test_captive_access_point_requires_a_password(self) -> None:
        wifi = source("src/WiFiManager.cpp")
        self.assertRegex(wifi, r"WiFi\.softAP\(\s*CAPTIVE_AP_SSID\s*,")

    @unittest.expectedFailure
    def test_openweathermap_transport_is_https(self) -> None:
        weather = source("src/WeatherManager.cpp")
        self.assertNotIn("http://api.openweathermap.org", weather)
        self.assertIn("https://api.openweathermap.org", weather)

    def test_sd_handler_rejects_path_traversal_and_api_shadowing(self) -> None:
        handler = source("src/SdStaticHandler.cpp")
        self.assertIn('requestPath.indexOf("..")', handler)
        self.assertIn("requestPath.indexOf('\\\\')", handler)
        self.assertIn('requestPath.startsWith("/api/")', handler)

    def test_storage_diagnostics_disable_client_cache(self) -> None:
        handler = source("src/SdStaticHandler.cpp")
        self.assertIn("no-store, no-cache, must-revalidate", handler)

    def test_weather_request_has_a_bounded_timeout(self) -> None:
        weather = source("src/WeatherManager.cpp")
        match = re.search(r"http\.setTimeout\((\d+)\)", weather)
        self.assertIsNotNone(match)
        self.assertLessEqual(int(match.group(1)), 10_000)

    def test_sensitive_configuration_routes_use_json_handlers(self) -> None:
        web = source("src/WebManager.cpp")
        for route in ("/api/wifi", "/api/owm", "/api/ntp", "/api/system"):
            self.assertIn(f'POST_JSON("{route}"', web)


if __name__ == "__main__":
    unittest.main(verbosity=2)
