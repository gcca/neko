#pragma once

#include <drogon/HttpController.h>

#include "../../handling.hpp"

namespace neko::handling::stories {

class Stories : public drogon::HttpController<Stories> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(Stories::List, "/stories/list", drogon::Get);
    ADD_METHOD_TO(Stories::Stylesheet, "/stories/stories.css", drogon::Get);
    METHOD_LIST_END

    void List(const drogon::HttpRequestPtr& request, Respond&& respond) const;
    void Stylesheet(const drogon::HttpRequestPtr& request, Respond&& respond) const;
};

}  // namespace neko::handling::stories
