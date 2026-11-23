#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <arpa/inet.h>
#include <drogon/HttpAppFramework.h>

#include "handling/a/handlers.hpp"
#include "handling/index/handlers.hpp"
#include "neko/core/conf.hpp"

ABSL_FLAG(std::string, host, "127.0.0.1", "IPv4 or IPv6 address to listen on");
ABSL_FLAG(int, port, 8080, "Port to listen on (1-65535)");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Neko web server");
  const auto positional = absl::ParseCommandLine(argc, argv);
  if (positional.size() > 1) {
    std::cerr << "Unexpected positional argument: " << positional[1] << '\n';
    return EXIT_FAILURE;
  }

  const auto host = absl::GetFlag(FLAGS_host);
  in_addr ipv4{};
  in6_addr ipv6{};
  if (inet_pton(AF_INET, host.c_str(), &ipv4) != 1 &&
      inet_pton(AF_INET6, host.c_str(), &ipv6) != 1) {
    std::cerr << "--host must be a literal IPv4 or IPv6 address\n";
    return EXIT_FAILURE;
  }

  const int port = absl::GetFlag(FLAGS_port);
  if (port < 1 || port > 65535) {
    std::cerr << "--port must be between 1 and 65535\n";
    return EXIT_FAILURE;
  }

  neko::core::conf::Load();

  auto &app = drogon::app();
  neko::handling::index::Register(app);
  neko::handling::a::Register(app, neko::core::conf::settings.dbpath);
  app.setFileTypes({}).setImplicitPageEnable(false);
  app.addListener(host, static_cast<std::uint16_t>(port)).run();
  return EXIT_SUCCESS;
}
