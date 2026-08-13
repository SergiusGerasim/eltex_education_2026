#pragma once

#include <signal.h>

int signal_handler_install(void);
sig_atomic_t signal_handler_stop_requested(void);
