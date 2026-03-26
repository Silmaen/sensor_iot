#include "module_registry.h"
#include <string.h>

void ModuleRegistry::init() {
    num_metrics = 0;
    num_commands = 0;
    for (size_t i = 0; i < MAX_METRICS; i++) {
        metrics[i] = nullptr;
        metric_units[i] = nullptr;
    }
    for (size_t i = 0; i < MAX_COMMANDS; i++) {
        commands[i] = nullptr;
        handlers[i] = nullptr;
        command_params[i] = nullptr;
        command_param_counts[i] = 0;
    }
}

bool ModuleRegistry::add_metric(const char* name, const char* unit) {
    if (num_metrics >= MAX_METRICS) return false;
    metrics[num_metrics] = name;
    metric_units[num_metrics] = unit;
    num_metrics++;
    return true;
}

bool ModuleRegistry::add_command(const char* name, CommandHandler handler,
                                 const CommandParamDef* params, size_t num_params) {
    if (num_commands >= MAX_COMMANDS) return false;
    commands[num_commands] = name;
    handlers[num_commands] = handler;
    command_params[num_commands] = params;
    command_param_counts[num_commands] = num_params;
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
