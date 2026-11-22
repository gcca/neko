#include "handlers.hpp"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

#include <gmock/gmock.h>
#include <sqlite3.h>

namespace neko::handling::a {
namespace {

using testing::HasSubstr;
using testing::Not;

class PostsPage : public testing::Test {
protected:
    void SetUp() override {
        auto directory = (std::filesystem::temp_directory_path() / "neko-test.XXXXXX").string();
        ASSERT_NE(mkdtemp(directory.data()), nullptr);
        scratch_ = directory;
        dbPath_ = scratch_ / "posts.db";

        sqlite3* handle = nullptr;
        const int opened = sqlite3_open(dbPath_.string().c_str(), &handle);
        db_.reset(handle);
        ASSERT_EQ(opened, SQLITE_OK);

        std::ifstream schemaFile(NEKO_SCHEMA_PATH);
        ASSERT_TRUE(schemaFile.is_open());
        std::ostringstream schema;
        schema << schemaFile.rdbuf();
        execute(schema.str());
    }

    void TearDown() override {
        db_.reset();
        if (!scratch_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(scratch_, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void execute(const std::string& sql) {
        ASSERT_EQ(sqlite3_exec(db_.get(), sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK)
            << sqlite3_errmsg(db_.get());
    }

    std::filesystem::path scratch_;
    std::filesystem::path dbPath_;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_{nullptr, sqlite3_close};
};

TEST_F(PostsPage, OrdersPostsByTimestampThenId) {
    execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader', '!fixture');
        INSERT INTO a_post (id, title, created_by, created_at) VALUES
            (1, 'First tie', 1, 1700100000),
            (2, 'Second tie', 1, 1700100000),
            (3, 'Older post', 1, 1700000000),
            (4, 'Newest post', 1, 1700200000);
    )sql");

    const auto response = postsResponse(dbPath_);
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

TEST_F(PostsPage, EscapesStoredTextAndRendersOnlyWebLinks) {
    execute(R"sql(
        INSERT INTO auth_user (id, username, password) VALUES (1, 'reader & friend', '!fixture');
        INSERT INTO a_post (title, body, link, created_by) VALUES
            ('A <strong> title', 'Notes about "A" & "B"', 'https://example.test/?a=1&b=2', 1),
            ('Text discussion', '', NULL, 1),
            ('Unsafe scheme', '', 'javascript:void(0)', 1),
            ('Relative URL', '', '//example.test/', 1);
    )sql");

    const auto response = postsResponse(dbPath_);
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

TEST_F(PostsPage, ShowsAnEmptyStateForAnEmptyDatabase) {
    const auto response = postsResponse(dbPath_);
    EXPECT_EQ(response->getStatusCode(), drogon::k200OK);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("No posts yet."));
}

TEST_F(PostsPage, ReturnsUnavailableForAMissingDatabaseWithoutCreatingIt) {
    const auto missing = scratch_ / "missing.db";
    const auto response = postsResponse(missing);
    EXPECT_EQ(response->getStatusCode(), drogon::k503ServiceUnavailable);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("Posts are temporarily unavailable."));
    EXPECT_FALSE(std::filesystem::exists(missing));
}

TEST_F(PostsPage, ReturnsUnavailableForAnUninitializedDatabase) {
    execute("DROP TABLE a_post;");
    const auto response = postsResponse(dbPath_);
    EXPECT_EQ(response->getStatusCode(), drogon::k503ServiceUnavailable);
    EXPECT_THAT(std::string(response->getBody()), HasSubstr("Posts are temporarily unavailable."));
}

}  // namespace
}  // namespace neko::handling::a
