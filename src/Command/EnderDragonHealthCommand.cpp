#include "Command/EnderDragonHealthCommand.h"

#include "Event/EnderDragonHealth.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"

namespace my_mod::command {

namespace {
bool commandRegistered = false;
}

void registerEnderDragonHealthCommand() {
    if (commandRegistered) {
        return;
    }

    auto& cmd = ll::command::CommandRegistrar::getInstance(false)
                    .getOrCreateCommand(
                        "enderdragonhealth",
                        "Manually apply configured max health to existing ender dragons.",
                        CommandPermissionLevel::GameDirectors
                    )
                    .alias("edhealth");

    cmd.overload().execute([](CommandOrigin const&, CommandOutput& output) {
        int const updatedCount = event::applyEnderDragonMaxHealthToExisting();
        output.success("Applied configured max health to {} ender dragon(s).", updatedCount);
    });

    cmd.overload().text("apply").execute([](CommandOrigin const&, CommandOutput& output) {
        int const updatedCount = event::applyEnderDragonMaxHealthToExisting();
        output.success("Applied configured max health to {} ender dragon(s).", updatedCount);
    });

    commandRegistered = true;
}

void unregisterEnderDragonHealthCommand() {
    // CommandRegistrar does not expose per-command unregister in plugin scope.
}

} // namespace my_mod::command
