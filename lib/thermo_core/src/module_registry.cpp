#include "module_registry.h"
#include <string.h>

void ModuleRegistry::init() {
    num_metrics = 0;
    num_commands = 0;
    for (size_t i = 0; i < MAX_METRICS; i++) metrics[i] = nullptr;
    for (size_t i = 0; i < MAX_COMMANDS; i++) {
        commands[i] = nullptr;
        handlers[i] = nullptr;
    }
}

bool ModuleRegistry::add_metric(const char* name) {
    if (num_metrics >= MAX_METRICS) return false;
    metrics[num_metrics++] = name;
    return true;
}

bool ModuleRegistry::add_command(const char* name, CommandHandler handler) {
    if (num_commands >= MAX_COMMANDS) return false;
    commands[num_commands] = name;
    handlers[num_commands] = handler;
    num_commands++;
    return true;
}

bool ModuleRegistry::dispatch(const char* action, const char* payload) const {
    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(action, commands[i]) == 0 && handlers[i] != nullptr) {
            return handlers[i](payload);
        }
    }
    return false;
}
