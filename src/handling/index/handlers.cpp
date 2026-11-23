#include "handlers.hpp"

namespace neko::handling::index {

void Index::RedirectToStories(const drogon::HttpRequestPtr&, Respond&& respond) const {
    respond(drogon::HttpResponse::newRedirectionResponse("/stories/list"));
}

void Index::Healthcheck(const drogon::HttpRequestPtr&, Respond&& respond) const {
    auto response = drogon::HttpResponse::newHttpResponse(
        drogon::k200OK, drogon::CT_TEXT_PLAIN);
    response->setBody(u8"😎");
    respond(response);
}

}  // namespace neko::handling::index
