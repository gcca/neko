#pragma once

#include <string>

namespace neko::core::conf {

extern struct Settings {
  std::string dbpath = "data/neko.db";
} settings;

void Load();

} // namespace neko::core::conf
