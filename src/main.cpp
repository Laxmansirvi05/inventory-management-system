// main.cpp : read settings from environment variables, connect to MySQL, load products, start the server.
//   DB_HOST DB_PORT DB_USER DB_PASSWORD DB_NAME   database
//   DB_SSL_CA (path to the CA certificate) or DB_SSL_CA_PEM (the certificate text itself, for hosts without files)
//   PORT (default 8080)  PUBLIC_DIR (default public)  SHOP_NAME  COOKIE_SECURE=1 (when behind HTTPS)
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "server.h"

static std::string env(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

int main() {
    App app;
    app.publicDir = env("PUBLIC_DIR", "public");
    app.shopName = env("SHOP_NAME", "Inventory Store");
    app.secureCookie = env("COOKIE_SECURE", "0") == "1";

    DbConfig cfg = DbConfig::fromEnv();
    // Hosting services (Render) pass the Aiven CA certificate easily as text, not as a file: write it out once.
    std::string pem = env("DB_SSL_CA_PEM", "");
    if (!pem.empty() && cfg.sslCa.empty()) {
        for (size_t at; (at = pem.find("\\n")) != std::string::npos;) pem.replace(at, 2, "\n");   // literal \n -> newline
        const char* path = "/tmp/ims-db-ca.pem";
        std::ofstream(path) << pem << "\n";
        chmod(path, 0600);
        cfg.sslCa = path;
    }
    std::string err;
    if (!app.db.connect(cfg, err)) {
        std::fprintf(stderr, "Cannot connect to MySQL at %s:%u (database %s): %s\n", cfg.host.c_str(), cfg.port, cfg.name.c_str(), err.c_str());
        return 1;
    }
    try {
        app.inv.replaceAll(loadProducts(app.db, nullptr));      // MySQL is the source of truth: load it into memory
        app.refreshNames();
    } catch (const DbError& e) {
        std::fprintf(stderr, "Start-up load failed: %s\n", e.what());
        return 1;
    }
    std::printf("Loaded %zu products from database '%s'\n", app.inv.size(), cfg.name.c_str());

    registerAuthRoutes(app);
    registerCatalogRoutes(app);
    registerStockRoutes(app);
    registerReportRoutes(app);
    return runServer(app, std::atoi(env("PORT", "8080").c_str()));
}
