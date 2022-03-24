# Extra Credit Implementation Overview

This assignment implements the extra credit `repeat` and `terminateall` commands.

## Usage

- `repeat [n] [program]`: Where `[program]` is the path to a program that will be started in the background and `[n]` is the number of processes that will be spawned.
  - Example: `repeat 4 /usr/bin/xterm` will open 4 terminals and return execution back to the application
- `terminateall`: This command doesn't take any additional arguments. It will terminate all PIDs started by this application with a `SIGTERM` signal.

## Implementation

To give a brief overview, here's how this assignment is structured:

- Grab user input from `std::cin` stopping to parse each line (that is, after we've read a `\n`)
- Take that line and split it into individual tokens.
  - For example, `repeat command -arg1 -arg2` would be parsed into an `std::vector<std::string>` containing `{"repeat", "command", "-arg1", "-arg2", "-arg2"}`
- After we have a list of tokens, we can pass that to the function `parseCommand()`, which takes the first token (the name of the command), and uses the rest of the (optional) tokens in that vector as arguments for that command. Each command is implemented as its own separate function (with the exception of `background`)

As mentioned above, each command has its own separate function. For example, when `start` is called, the function `executeStartCommand()` is run. Because of this structure, that allows `repeat` to simply call `executeStartCommand()` n times. In order to keep track of PIDs, there's a global variable called `pids` (an unordered set). Any time `executeStartCommand()` is called, the PID of the newly spawned process gets added to that set. When the process terminates, that PID is removed from the set. When `terminateall` is called, it loops over each PID in that set and calls `terminateProcess()` on that PID. `terminateProcess()`. `terminateProcess()` tries to gracefully terminate the given PID by sending a `SIGTERM` signal. After all PIDs are terminated, the set containing all PIDs is cleared.

**Quick note**: `executeStartCommand()` has a boolean parameter called `background`. If true, the program will spawn a process then return control back to the application, otherwise, it'll use `waitpid` to wait for the spawned process to finish executing before returning control back to the main application.
