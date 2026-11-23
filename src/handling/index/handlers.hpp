#pragma once

#include <drogon/HttpController.h>

#include "../../handling.hpp"

namespace neko::handling::index {

class Index : public drogon::HttpController<Index> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(Index::RedirectToStories, "/", drogon::Get);
    ADD_METHOD_TO(Index::Healthcheck, "/healthcheck", drogon::Get);
    METHOD_LIST_END

    void RedirectToStories(const drogon::HttpRequestPtr& request, Respond&& respond) const;
    void Healthcheck(const drogon::HttpRequestPtr& request, Respond&& respond) const;
};

}  // namespace neko::handling::index
