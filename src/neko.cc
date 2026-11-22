#include "handling/a/handlers.hpp"
#include "handling/index/handlers.hpp"
#include "neko/core/conf.hpp"

#include <drogon/HttpAppFramework.h>

int main() {
    neko::core::conf::Load();

    auto& app = drogon::app();
    neko::handling::index::registerHandlers(app);
    neko::handling::a::registerHandlers(app, neko::core::conf::settings.dbpath);
    app.setFileTypes({}).setImplicitPageEnable(false);
    app.addListener("127.0.0.1", 8080).run();
}
