#pragma once

int signal_handlers_install(void);
void signal_handlers_restore(void);
int signal_stop_requested(void);
