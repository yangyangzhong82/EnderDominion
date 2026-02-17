#pragma once

#include "Config/Config.h"
#include "ll/api/io/Logger.h"

namespace my_mod {

[[nodiscard]] ll::io::Logger& getLogger();
[[nodiscard]] Config&          getConfig();

} // namespace my_mod
