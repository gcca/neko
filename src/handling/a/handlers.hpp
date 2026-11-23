#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <drogon/HttpResponse.h>

namespace drogon {
class HttpAppFramework;
}

namespace neko::handling::a {

struct Post {
    std::string title;
    std::string body;
    std::optional<std::string> link;
    std::string submitter;
    std::string date;
};

drogon::HttpResponsePtr postsResponse(const std::filesystem::path& dbPath);
void Register(drogon::HttpAppFramework& app, std::filesystem::path dbPath);

}  // namespace neko::handling::a
