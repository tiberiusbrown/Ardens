#pragma once

// Runs the command-line-only simulation when a headless=<milliseconds>
// argument is present. Returns true when the argument was present and stores
// the process exit status in exit_code.
bool run_headless_if_requested(int argc, char** argv, int& exit_code);
