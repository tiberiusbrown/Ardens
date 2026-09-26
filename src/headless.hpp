#pragma once

#include "cli_args.hpp"

// Runs the command-line-only simulation when the parsed options contain
// headless. Returns true when the option was present and stores
// the process exit status in exit_code.
bool run_headless_if_requested(std::vector<cli_arg_t> const& args, int& exit_code);
