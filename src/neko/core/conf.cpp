#include "conf.hpp"

#include <cstdlib>

namespace neko::core::conf {

Settings settings;

void Load() {
  if (const char *dbPath = std::getenv("DBPATH"); dbPath && *dbPath) {
    settings.dbpath = dbPath;
  }
}

} // namespace neko::core::conf
