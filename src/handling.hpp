#pragma once

#include <functional>

#include <drogon/HttpResponse.h>

namespace neko::handling {

using Respond = std::function<void(const drogon::HttpResponsePtr&)>;

}  // namespace neko::handling
