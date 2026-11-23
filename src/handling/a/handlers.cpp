#include "handlers.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpViewData.h>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

namespace neko::handling::a {
namespace {

using Respond = std::function<void(const drogon::HttpResponsePtr&)>;

std::string columnText(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    if (!text) {
        return {};
    }
    return {reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(sqlite3_column_bytes(statement, column))};
}

std::optional<std::string> articleLink(std::string url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos ||
        std::any_of(url.begin(), url.end(), [](unsigned char ch) {
            return ch <= ' ' || ch == '\x7f' || ch == '\\';
        })) {
        return std::nullopt;
    }

    auto scheme = url.substr(0, schemeEnd);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char ch) { return std::tolower(ch); });
    const std::string_view remainder = std::string_view(url).substr(schemeEnd + 3);
    const auto authority = remainder.substr(0, remainder.find_first_of("/?#"));
    if ((scheme != "http" && scheme != "https") || authority.empty() ||
        authority.front() == ':' || authority.find('@') != std::string_view::npos) {
        return std::nullopt;
    }
    return url;
}

std::vector<Post> loadPosts(const std::filesystem::path& dbPath) {
    sqlite3* handle = nullptr;
    const int opened = sqlite3_open_v2(dbPath.string().c_str(), &handle,
                                       SQLITE_OPEN_READONLY, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(handle, sqlite3_close);
    if (opened != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errstr(opened));
    }

    constexpr char query[] = R"sql(
        SELECT p.title, p.body, p.link, u.username,
               strftime('%Y-%m-%d', p.created_at, 'unixepoch')
        FROM a_post AS p
        JOIN auth_user AS u ON u.id = p.created_by
        ORDER BY p.created_at DESC, p.id DESC
    )sql";
    sqlite3_stmt* statement = nullptr;
    const int prepared = sqlite3_prepare_v2(db.get(), query, -1, &statement, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> rows(
        statement, sqlite3_finalize);
    if (prepared != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db.get()));
    }

    std::vector<Post> posts;
    int result = sqlite3_step(rows.get());
    while (result == SQLITE_ROW) {
        posts.push_back({columnText(rows.get(), 0), columnText(rows.get(), 1),
                         articleLink(columnText(rows.get(), 2)), columnText(rows.get(), 3),
                         columnText(rows.get(), 4)});
        result = sqlite3_step(rows.get());
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db.get()));
    }
    return posts;
}

void postsStylesheet(const drogon::HttpRequestPtr&, Respond&& respond) {
    respond(drogon::HttpResponse::newFileResponse(NEKO_POSTS_CSS_PATH, "",
                                                  drogon::CT_TEXT_CSS));
}

}  // namespace

drogon::HttpResponsePtr postsResponse(const std::filesystem::path& dbPath) {
    drogon::HttpViewData data;
    bool unavailable = false;
    try {
        data.insert("posts", loadPosts(dbPath));
    } catch (const std::runtime_error& error) {
        LOG_ERROR << "Could not load posts: " << error.what();
        unavailable = true;
    }
    data.insert("unavailable", unavailable);

    auto response = drogon::HttpResponse::newHttpViewResponse("APosts", data);
    if (unavailable) {
        response->setStatusCode(drogon::k503ServiceUnavailable);
    }
    return response;
}

void Register(drogon::HttpAppFramework& app, std::filesystem::path dbPath) {
    app.registerHandler(
        "/a",
        [dbPath = std::move(dbPath)](const drogon::HttpRequestPtr&, Respond&& respond) {
            respond(postsResponse(dbPath));
        },
        {drogon::Get});
    app.registerHandler("/a/posts.css", &postsStylesheet, {drogon::Get});
}

}  // namespace neko::handling::a
