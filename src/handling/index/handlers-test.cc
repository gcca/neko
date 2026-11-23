#include "handlers.hpp"

#include <gmock/gmock.h>

namespace neko::handling::index {
namespace {

TEST(Neko, IndexRedirectsToStories) {
    testing::MockFunction<Respond> respond;
    EXPECT_CALL(respond, Call(testing::NotNull()))
        .WillOnce([](const drogon::HttpResponsePtr& response) {
            EXPECT_EQ(response->getStatusCode(), drogon::k302Found);
            EXPECT_EQ(response->getHeader("location"), "/stories/list");
        });

    Index index;
    index.RedirectToStories(drogon::HttpRequest::newHttpRequest(), respond.AsStdFunction());
}

TEST(Neko, HealthcheckReturnsSunglassesEmoji) {
    testing::MockFunction<Respond> respond;
    EXPECT_CALL(respond, Call(testing::NotNull()))
        .WillOnce([](const drogon::HttpResponsePtr& response) {
            EXPECT_EQ(response->getStatusCode(), drogon::k200OK);
            EXPECT_EQ(response->getContentType(), drogon::CT_TEXT_PLAIN);
            EXPECT_EQ(response->getBody(), u8"😎");
        });

    Index index;
    index.Healthcheck(drogon::HttpRequest::newHttpRequest(), respond.AsStdFunction());
}

}  // namespace
}  // namespace neko::handling::index
