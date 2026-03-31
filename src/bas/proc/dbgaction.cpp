/*
 * Debug actions: use scriptable UI stack for "do" command.
 * List/do iterate stack (top first) and call scriptable get_action_list / get_action.
 */

#define _POSIX_C_SOURCE 200809L

#include "../script/scriptable.hpp"

#include <bas/proc/dbgaction.h>

#include <stdlib.h>

void dbgaction_list(FILE *out, const char *prefix) {
    for (auto& name : script_app.findVars(prefix)) {
        fprintf(out, "  var %s\n", name.c_str());
    }
    for (auto& name : script_app.findMethods(prefix)) {
        fprintf(out, "  method %s\n", name.c_str());
    }
}

int dbgaction_do(const char *name, const char* const* argv, int argc) {
    if (!name || !*name) return -1;

    if (!script_app.hasMethod(name)) {
        fprintf(stderr, "  method %s not found\n", name);
        return -1;
    }

    return script_app.invokeMethod(name, argv, argc);
}
