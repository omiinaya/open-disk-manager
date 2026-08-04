#include "opm/i18n.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

namespace opm {
namespace i18n {

namespace {

std::mutex g_mutex;
std::string g_locale = "en";
// locale -> (key -> localized text)
std::unordered_map<std::string,
                   std::unordered_map<std::string, std::string>> g_catalogs;

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Parse a "key = value" catalog entry; returns false for comments/blank lines.
bool parseLine(const std::string& raw, std::string& key, std::string& value) {
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#') return false;
    size_t eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    value = trim(line.substr(eq + 1));
    return !key.empty();
}

} // anonymous namespace

std::string currentLocale() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_locale;
}

void setLocale(const std::string& locale) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_locale = locale.empty() ? "en" : locale;
}

int loadCatalog(const std::string& locale, const std::string& path) {
    std::ifstream file(path);
    if (!file) return -1;
    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return loadCatalogFromString(locale, contents);
}

int loadCatalogFromString(const std::string& locale,
                          const std::string& contents) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& catalog = g_catalogs[locale.empty() ? "en" : locale];
    std::istringstream stream(contents);
    std::string line;
    int count = 0;
    while (std::getline(stream, line)) {
        std::string key, value;
        if (parseLine(line, key, value)) {
            catalog[key] = value;
            count++;
        }
    }
    return count;
}

std::string tr(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto locale_it = g_catalogs.find(g_locale);
    if (locale_it != g_catalogs.end()) {
        const auto& catalog = locale_it->second;
        const auto it = catalog.find(key);
        if (it != catalog.end()) return it->second;
    }
    // Fall back to the source ("en") catalog
    if (g_locale != "en") {
        const auto en_it = g_catalogs.find("en");
        if (en_it != g_catalogs.end()) {
            const auto it = en_it->second.find(key);
            if (it != en_it->second.end()) return it->second;
        }
    }
    return key;
}

std::string trf(const std::string& key,
                const std::vector<std::string>& args) {
    std::string text = tr(key);
    for (size_t i = 0; i < args.size(); i++) {
        std::string placeholder = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = text.find(placeholder, pos)) != std::string::npos) {
            text.replace(pos, placeholder.size(), args[i]);
            pos += args[i].size();
        }
    }
    return text;
}

std::vector<std::string> loadedLocales() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<std::string> locales;
    locales.reserve(g_catalogs.size());
    for (const auto& [locale, catalog] : g_catalogs) {
        (void)catalog;
        locales.push_back(locale);
    }
    std::sort(locales.begin(), locales.end());
    return locales;
}

void clearCatalogs() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_catalogs.clear();
}

} // namespace i18n
} // namespace opm
