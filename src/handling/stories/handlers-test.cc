#include "handlers.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include <drogon/HttpAppFramework.h>
#include <gmock/gmock.h>
#include <sqlite3.h>

#include "neko/core/conf.hpp"

namespace neko::handling::stories {
namespace {

using testing::HasSubstr;
using testing::Not;

TEST(NekoRoutes, RegistersControllerPaths) {
    using testing::_;
    using testing::FieldsAre;
    EXPECT_THAT(drogon::app().getHandlersInfo(), testing::UnorderedElementsAre(
        FieldsAre("/", drogon::Get, _),
        FieldsAre("/healthcheck", drogon::Get, _),
        FieldsAre("/stories/list", drogon::Get, _),
        FieldsAre("/stories/stories.css", drogon::Get, _)));
}

class StoriesPage : public testing::Test {
protected:
    void SetUp() override {
        auto directory = (std::filesystem::temp_directory_path() / "neko-test.XXXXXX").string();
        ASSERT_NE(mkdtemp(directory.data()), nullptr);
        scratch_ = directory;
        dbPath_ = scratch_ / "stories.db";

        sqlite3* handle = nullptr;
        const int opened = sqlite3_open(dbPath_.string().c_str(), &handle);
        db_.reset(handle);
        ASSERT_EQ(opened, SQLITE_OK);

        std::ifstream schemaFile(NEKO_SCHEMA_PATH);
        ASSERT_TRUE(schemaFile.is_open());
        std::ostringstream schema;
        schema << schemaFile.rdbuf();
        Execute(schema.str());
    }

    void TearDown() override {
        neko::core::conf::settings.dbpath.swap(previousDbPath_);
        db_.reset();
        if (!scratch_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(scratch_, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void Execute(const std::string& sql) {
        ASSERT_EQ(sqlite3_exec(db_.get(), sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK)
            << sqlite3_errmsg(db_.get());
    }

    drogon::HttpResponsePtr ListResponse(const std::filesystem::path& dbPath) {
        neko::core::conf::settings.dbpath = dbPath.string();
        drogon::HttpResponsePtr response;
        Stories{}.List(drogon::HttpRequest::newHttpRequest(),
                       [&response](const drogon::HttpResponsePtr& value) { response = value; });
        return response;
    }

    std::string previousDbPath_ = neko::core::conf::settings.dbpath;
    std::filesystem::path scratch_;
    std::filesystem::path dbPath_;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_{nullptr, sqlite3_close};
};

TEST_F(StoriesPage, OrdersStoriesByTimestampThenId) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO story (id, title, created_by, created_at) VALUES
            (1, 'First tie', 1, 1700100000),
            (2, 'Second tie', 1, 1700100000),
            (3, 'Older post', 1, 1700000000),
            (4, 'Newest post', 1, 1700200000);
    )sql");

    const auto response = ListResponse(dbPath_);
    ASSERT_EQ(response->getStatusCode(), drogon::k200OK);
    EXPECT_EQ(response->getContentType(), drogon::CT_TEXT_HTML);
    const auto body = response->getBody();
    auto previous = body.find("Newest post");
    ASSERT_NE(previous, std::string_view::npos);
    for (const auto* title : {"Second tie", "First tie", "Older post"}) {
        const auto position = body.find(title);
        ASSERT_NE(position, std::string_view::npos) << title;
        EXPECT_LT(previous, position) << title;
        previous = position;
    }
}

TEST_F(StoriesPage, EscapesStoredTextAndRendersOnlyWebLinks) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader & friend', '!fixture');
        INSERT INTO story (title, body, link, created_by) VALUES
            ('A <strong> title', 'Notes about "A" & "B"', 'https://example.test/?a=1&b=2', 1),
            ('Text discussion', '', NULL, 1),
            ('Unsafe scheme', '', 'javascript:void(0)', 1),
            ('Relative URL', '', '//example.test/', 1);
    )sql");

    const auto response = ListResponse(dbPath_);
    const std::string body(response->getBody());
    EXPECT_THAT(body, HasSubstr("href=\"https://example.test/?a=1&amp;b=2\">A &lt;strong&gt; title</a>"));
    EXPECT_THAT(body, HasSubstr("Notes about &quot;A&quot; &amp; &quot;B&quot;"));
    EXPECT_THAT(body, HasSubstr("reader &amp; friend"));
    EXPECT_THAT(body, HasSubstr("Text discussion"));
    EXPECT_THAT(body, HasSubstr("Unsafe scheme"));
    EXPECT_THAT(body, HasSubstr("Relative URL"));
    EXPECT_THAT(body, Not(HasSubstr("Text discussion</a>")));
    EXPECT_THAT(body, Not(HasSubstr("href=\"javascript:")));
    EXPECT_THAT(body, Not(HasSubstr("href=\"//example.test/")));
}

TEST_F(StoriesPage, RendersHttpLinksWithLowercaseAndUppercaseSchemes) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO story (title, link, created_by) VALUES
            ('Plain HTTP', 'http://example.test/article', 1),
            ('Uppercase HTTP', 'HTTP://example.test/article', 1);
    )sql");

    const auto response = ListResponse(dbPath_);
    ASSERT_EQ(response->getStatusCode(), drogon::k200OK);
    const std::string body(response->getBody());
    EXPECT_THAT(body, HasSubstr("href=\"http://example.test/article\">Plain HTTP</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"HTTP://example.test/article\">Uppercase HTTP</a>"));
}

TEST_F(StoriesPage, RendersMalformedLinksAsPlainText) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO story (title, link, created_by) VALUES
            ('Unsupported scheme', 'ftp://example.test/article', 1),
            ('Whitespace', 'https://example.test/a b', 1),
            ('DEL character', 'https://example.test/a' || char(127) || 'b', 1),
            ('Backslash', 'https://example.test/a' || char(92) || 'b', 1),
            ('Missing authority', 'https:///article', 1),
            ('Port-only authority', 'https://:443/article', 1),
            ('User info', 'https://reader@example.test/article', 1);
    )sql");

    const auto response = ListResponse(dbPath_);
    ASSERT_EQ(response->getStatusCode(), drogon::k200OK);
    const std::string body(response->getBody());
    for (const auto* title : {"Unsupported scheme", "Whitespace", "DEL character",
                              "Backslash", "Missing authority", "Port-only authority",
                              "User info"}) {
        SCOPED_TRACE(title);
        EXPECT_THAT(body, HasSubstr(title));
        EXPECT_THAT(body, Not(HasSubstr(std::string(title) + "</a>")));
    }
}

TEST_F(StoriesPage, RendersInvalidPortsAsPlainText) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO story (title, link, created_by) VALUES
            ('Nonnumeric port', 'https://example.test:http/article', 1),
            ('Partially numeric port', 'https://example.test:80a/article', 1),
            ('Out-of-range port', 'https://example.test:65536/article', 1),
            ('Overflowing port', 'https://example.test:99999999999999999999/article', 1),
            ('Bracketed nonnumeric port', 'https://[::1]:x/article', 1),
            ('Bracketed out-of-range port', 'https://[2001:db8::1]:70000/article', 1);
    )sql");

    const auto response = ListResponse(dbPath_);
    ASSERT_EQ(response->getStatusCode(), drogon::k200OK);
    const std::string body(response->getBody());
    for (const auto* title : {"Nonnumeric port", "Partially numeric port", "Out-of-range port",
                              "Overflowing port", "Bracketed nonnumeric port",
                              "Bracketed out-of-range port"}) {
        SCOPED_TRACE(title);
        EXPECT_THAT(body, HasSubstr(title));
        EXPECT_THAT(body, Not(HasSubstr(std::string(title) + "</a>")));
    }
}

TEST_F(StoriesPage, RendersLinksWithValidPorts) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO story (title, link, created_by) VALUES
            ('Hostname with port', 'https://example.test:8080/article', 1),
            ('Highest port', 'http://example.test:65535/', 1),
            ('Port zero', 'http://example.test:0/', 1),
            ('Empty port', 'https://example.test:/article', 1),
            ('IPv6 with port', 'https://[::1]:8443/article', 1),
            ('IPv6 without port', 'http://[2001:db8::1]/article', 1);
    )sql");

    const auto response = ListResponse(dbPath_);
    ASSERT_EQ(response->getStatusCode(), drogon::k200OK);
    const std::string body(response->getBody());
    EXPECT_THAT(body, HasSubstr("href=\"https://example.test:8080/article\">Hostname with port</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"http://example.test:65535/\">Highest port</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"http://example.test:0/\">Port zero</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"https://example.test:/article\">Empty port</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"https://[::1]:8443/article\">IPv6 with port</a>"));
    EXPECT_THAT(body, HasSubstr("href=\"http://[2001:db8::1]/article\">IPv6 without port</a>"));
}

TEST_F(StoriesPage, ShowsAnEmptyStateForAnEmptyDatabase) {
    const auto response = ListResponse(dbPath_);
    EXPECT_EQ(response->getStatusCode(), drogon::k200OK);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("No stories yet."));
}

TEST_F(StoriesPage, ReturnsUnavailableForAMissingDatabaseWithoutCreatingIt) {
    const auto missing = scratch_ / "missing.db";
    const auto response = ListResponse(missing);
    EXPECT_EQ(response->getStatusCode(), drogon::k503ServiceUnavailable);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("Stories are temporarily unavailable."));
    EXPECT_FALSE(std::filesystem::exists(missing));
}

TEST_F(StoriesPage, ReturnsUnavailableForAnUninitializedDatabase) {
    Execute("DROP TABLE story;");
    const auto response = ListResponse(dbPath_);
    EXPECT_EQ(response->getStatusCode(), drogon::k503ServiceUnavailable);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("Stories are temporarily unavailable."));
}

TEST_F(StoriesPage, ReturnsUnavailableWhenQueryExecutionFails) {
    Execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        DROP TABLE story;
        CREATE VIEW story AS
        SELECT 1 AS id, abs(CAST('-9223372036854775808' AS INTEGER)) AS title,
               '' AS body, NULL AS link, 1 AS created_by, 1700000000 AS created_at;
    )sql");

    const auto response = ListResponse(dbPath_);
    EXPECT_EQ(response->getStatusCode(), drogon::k503ServiceUnavailable);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("Stories are temporarily unavailable."));
}

}  // namespace
}  // namespace neko::handling::stories
