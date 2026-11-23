#include "handlers.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <drogon/HttpViewData.h>
#include <json/json.h>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#include "neko/core/conf.hpp"

namespace neko::handling::stories {
namespace {

std::string ColumnText(sqlite3_stmt* statement, int column) {
    const auto* text = sqlite3_column_text(statement, column);
    if (!text) {
        return {};
    }
    return {reinterpret_cast<const char*>(text),
            static_cast<std::size_t>(sqlite3_column_bytes(statement, column))};
}

bool HasValidPort(std::string_view authority) {
    std::string_view port;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos) {
            return false;
        }
        const auto rest = authority.substr(close + 1);
        if (!rest.empty() && rest.front() != ':') {
            return false;
        }
        port = rest.substr(rest.empty() ? 0 : 1);
    } else if (const auto colon = authority.find(':'); colon != std::string_view::npos) {
        port = authority.substr(colon + 1);
    }

    unsigned value = 0;
    for (const char ch : port) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(ch - '0');
        if (value > 65535) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> ArticleLink(std::string url) {
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
        authority.front() == ':' || authority.find('@') != std::string_view::npos ||
        !HasValidPort(authority)) {
        return std::nullopt;
    }
    return url;
}

Json::Value LoadStories(const std::filesystem::path& dbPath) {
    sqlite3* handle = nullptr;
    const int opened = sqlite3_open_v2(dbPath.string().c_str(), &handle,
                                       SQLITE_OPEN_READONLY, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(handle, sqlite3_close);
    if (opened != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errstr(opened));
    }

    constexpr char query[] = R"sql(
        SELECT s.title, s.body, s.link, u.username,
               strftime('%Y-%m-%d', s.created_at, 'unixepoch')
        FROM story AS s
        JOIN auth_user AS u ON u.id = s.created_by
        ORDER BY s.created_at DESC, s.id DESC
    )sql";
    sqlite3_stmt* statement = nullptr;
    const int prepared = sqlite3_prepare_v2(db.get(), query, -1, &statement, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> rows(
        statement, sqlite3_finalize);
    if (prepared != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db.get()));
    }

    Json::Value stories(Json::arrayValue);
    int result = sqlite3_step(rows.get());
    while (result == SQLITE_ROW) {
        Json::Value row(Json::objectValue);
        row["title"] = ColumnText(rows.get(), 0);
        row["body"] = ColumnText(rows.get(), 1);
        const auto link = ArticleLink(ColumnText(rows.get(), 2));
        row["link"] = link ? Json::Value(*link) : Json::Value();
        row["submitter"] = ColumnText(rows.get(), 3);
        row["date"] = ColumnText(rows.get(), 4);
        stories.append(std::move(row));
        result = sqlite3_step(rows.get());
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db.get()));
    }
    return stories;
}

drogon::HttpResponsePtr StoriesResponse(const std::filesystem::path& dbPath) {
    drogon::HttpViewData data;
    bool unavailable = false;
    try {
        data.insert("stories", LoadStories(dbPath));
    } catch (const std::runtime_error& error) {
        LOG_ERROR << "Could not load stories: " << error.what();
        unavailable = true;
    }
    data.insert("unavailable", unavailable);

    auto response = drogon::HttpResponse::newHttpViewResponse("StoriesList", data);
    if (unavailable) {
        response->setStatusCode(drogon::k503ServiceUnavailable);
    }
    return response;
}

}  // namespace

void Stories::List(const drogon::HttpRequestPtr&, Respond&& respond) const {
    respond(StoriesResponse(neko::core::conf::settings.dbpath));
}

void Stories::Stylesheet(const drogon::HttpRequestPtr&, Respond&& respond) const {
    respond(drogon::HttpResponse::newFileResponse(
        NEKO_STORIES_CSS_PATH, "", drogon::CT_TEXT_CSS));
}

}  // namespace neko::handling::stories
