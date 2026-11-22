#include "handlers.hpp"

#include <drogon/HttpAppFramework.h>

namespace neko::handling::index {

void redirectToA(const drogon::HttpRequestPtr&, Respond&& respond) {
    respond(drogon::HttpResponse::newRedirectionResponse("/a"));
}

void healthcheck(const drogon::HttpRequestPtr&, Respond&& respond) {
    auto response = drogon::HttpResponse::newHttpResponse(
        drogon::k200OK, drogon::CT_TEXT_PLAIN);
    response->setBody(u8"😎");
    respond(response);
}

void registerHandlers(drogon::HttpAppFramework& app) {
    app.registerHandler("/", &redirectToA, {drogon::Get});
    app.registerHandler("/healthcheck", &healthcheck, {drogon::Get});
}

}  // namespace neko::handling::index
