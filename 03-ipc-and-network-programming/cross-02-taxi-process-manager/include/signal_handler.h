#pragma once

#include <stdbool.h>

bool signal_handler_install(void);
bool signal_handler_restore_defaults(void);
int signal_handler_descriptor(void);
void signal_handler_drain(void);
void signal_handler_close(void);
bool signal_handler_stop_requested(void);
bool signal_handler_child_changed(void);
void signal_handler_clear_child_changed(void);
