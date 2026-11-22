#include "handlers.hpp"

#include <gmock/gmock.h>

namespace neko::handling::index {
namespace {

TEST(Neko, IndexRedirectsToA) {
    testing::MockFunction<Respond> respond;
    EXPECT_CALL(respond, Call(testing::NotNull()))
        .WillOnce([](const drogon::HttpResponsePtr& response) {
            EXPECT_EQ(response->getStatusCode(), drogon::k302Found);
            EXPECT_EQ(response->getHeader("location"), "/a");
        });

    redirectToA(drogon::HttpRequest::newHttpRequest(), respond.AsStdFunction());
}

TEST(Neko, HealthcheckReturnsSunglassesEmoji) {
    testing::MockFunction<Respond> respond;
    EXPECT_CALL(respond, Call(testing::NotNull()))
        .WillOnce([](const drogon::HttpResponsePtr& response) {
            EXPECT_EQ(response->getStatusCode(), drogon::k200OK);
            EXPECT_EQ(response->getContentType(), drogon::CT_TEXT_PLAIN);
            EXPECT_EQ(response->getBody(), u8"😎");
        });

    healthcheck(drogon::HttpRequest::newHttpRequest(), respond.AsStdFunction());
}

}  // namespace
}  // namespace neko::handling::index
