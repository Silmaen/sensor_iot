#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr size_t MAX_METRICS = 8;
static constexpr size_t MAX_COMMANDS = 4;

// Command handler: receives the full JSON payload, returns true if handled
using CommandHandler = bool (*)(const char* payload);

struct ModuleRegistry {
    const char* metrics[MAX_METRICS];
    size_t num_metrics;

    const char* commands[MAX_COMMANDS];
    CommandHandler handlers[MAX_COMMANDS];
    size_t num_commands;

    void init();
    bool add_metric(const char* name);
    bool add_command(const char* name, CommandHandler handler);
    bool dispatch(const char* action, const char* payload) const;
};
