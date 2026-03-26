#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr size_t MAX_METRICS = 8;
static constexpr size_t MAX_COMMANDS = 4;
static constexpr size_t MAX_COMMAND_PARAMS = 4;

// Command handler: receives the full JSON payload, returns true if handled
using CommandHandler = bool (*)(const char* payload);

// Describes one parameter of a command (for capabilities reporting)
struct CommandParamDef {
    const char* name;
    const char* type; // "number", "string", or "boolean"
};

struct ModuleRegistry {
    const char* metrics[MAX_METRICS];
    const char* metric_units[MAX_METRICS]; // nullable — unit string per metric
    size_t num_metrics;

    const char* commands[MAX_COMMANDS];
    CommandHandler handlers[MAX_COMMANDS];
    const CommandParamDef* command_params[MAX_COMMANDS]; // nullable — param array per command
    size_t command_param_counts[MAX_COMMANDS];
    size_t num_commands;

    void init();
    bool add_metric(const char* name, const char* unit = nullptr);
    bool add_command(const char* name, CommandHandler handler,
                     const CommandParamDef* params = nullptr, size_t num_params = 0);
    bool dispatch(const char* action, const char* payload) const;
};
