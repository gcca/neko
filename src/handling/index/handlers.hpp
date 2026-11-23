#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <functional>

namespace drogon {
class HttpAppFramework;
}

namespace neko::handling::index {

using Respond = std::function<void(const drogon::HttpResponsePtr&)>;

void redirectToA(const drogon::HttpRequestPtr& request, Respond&& respond);
void healthcheck(const drogon::HttpRequestPtr& request, Respond&& respond);
void Register(drogon::HttpAppFramework& app);

}  // namespace neko::handling::index
