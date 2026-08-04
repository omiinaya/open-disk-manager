#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace opm {
namespace i18n {

// ============================================================================
// Minimal internationalization framework
//
// Message catalogs are plain "key = value" text files (one message per line,
// '#' starts a comment). The framework keeps one catalog per locale in
// memory; the active locale decides which catalog is consulted. Missing
// keys fall back to the "en" (source) catalog, then to the key itself.
//
// Catalog file format:
//   # comment
//   hello = Hola
//   disk.label = Etiqueta del disco
// ============================================================================

// Locale helpers (ISO 639-1, optionally with region: "es", "es-MX")
std::string currentLocale();
void setLocale(const std::string& locale);

// Load a catalog file for the given locale ("es", "fr", ...).
// Returns the number of messages loaded, or -1 on file error.
int loadCatalog(const std::string& locale, const std::string& path);

// Load a catalog from an in-memory buffer (used by tests).
int loadCatalogFromString(const std::string& locale, const std::string& contents);

// Translate a key in the active locale, falling back to "en" then the key.
std::string tr(const std::string& key);

// Convenience: translate with printf-style argument substitution ({0}, {1}).
std::string trf(const std::string& key,
                const std::vector<std::string>& args = {});

// List of loaded locales
std::vector<std::string> loadedLocales();

// Clear all catalogs (for tests)
void clearCatalogs();

} // namespace i18n
} // namespace opm
