#include <gtest/gtest.h>
#include "opm/i18n.hpp"

using namespace opm;

TEST(I18nTest, MissingKeyFallsBackToKey) {
    i18n::clearCatalogs();
    i18n::setLocale("en");
    EXPECT_EQ(i18n::tr("no.such.key"), "no.such.key");
}

TEST(I18nTest, LoadCatalogFromString) {
    i18n::clearCatalogs();
    const char* catalog =
        "# comment line\n"
        "hello = Hello\n"
        "disk.label = Disk label\n";
    int count = i18n::loadCatalogFromString("en", catalog);
    EXPECT_EQ(count, 2);

    i18n::setLocale("en");
    EXPECT_EQ(i18n::tr("hello"), "Hello");
    EXPECT_EQ(i18n::tr("disk.label"), "Disk label");
}

TEST(I18nTest, LocaleSwitchingAndFallback) {
    i18n::clearCatalogs();
    i18n::loadCatalogFromString("en", "hello = Hello\nbye = Bye\n");
    i18n::loadCatalogFromString("es",
        "hello = Hola\n"
        "extra = Extra\n");

    i18n::setLocale("es");
    EXPECT_EQ(i18n::tr("hello"), "Hola");
    EXPECT_EQ(i18n::tr("extra"), "Extra");
    // "bye" is missing in es -> falls back to the en catalog
    EXPECT_EQ(i18n::tr("bye"), "Bye");
    // missing everywhere -> key itself
    EXPECT_EQ(i18n::tr("missing"), "missing");
}

TEST(I18nTest, FormatArguments) {
    i18n::clearCatalogs();
    i18n::loadCatalogFromString("en",
        "create.ok = Partition {0} created ({1})\n");
    i18n::setLocale("en");
    EXPECT_EQ(i18n::trf("create.ok", {"3", "20.00 MB"}),
              "Partition 3 created (20.00 MB)");
}

TEST(I18nTest, InvalidFileReturnsError) {
    i18n::clearCatalogs();
    EXPECT_EQ(i18n::loadCatalog("es", "/nonexistent/catalog.po"), -1);
}

TEST(I18nTest, LoadedLocales) {
    i18n::clearCatalogs();
    i18n::loadCatalogFromString("en", "a = A\n");
    i18n::loadCatalogFromString("fr", "a = À\n");
    auto locales = i18n::loadedLocales();
    ASSERT_EQ(locales.size(), 2u);
    EXPECT_EQ(locales[0], "en");
    EXPECT_EQ(locales[1], "fr");
}

// The shipped catalogs (es/fr/de/zh/ja) must all load and translate.
TEST(I18nTest, ShippedCatalogsLoadAndTranslate) {
    i18n::clearCatalogs();
    std::string dir = std::string(OPM_SOURCE_DIR) + "/i18n/";
    for (const char* loc : {"es", "fr", "de", "zh", "ja"}) {
        i18n::clearCatalogs();
        int n = i18n::loadCatalog(loc, dir + loc + ".po");
        EXPECT_GT(n, 0) << loc;
        i18n::setLocale(loc);
        // The key must resolve to a translation, never the raw key.
        EXPECT_NE(i18n::tr("cl.list.title"), "cl.list.title") << loc;
        EXPECT_NE(i18n::tr("cl.create.ok"), "cl.create.ok") << loc;
    }
}
