# Power and diagnostics

`host::Config::power` limits the frame rate. `maxFps` caps it, and `lowPower` is polled every vsync; while it returns true, the host runs at `lowPowerFps` (30 by default). Bridge it to the OS battery saver or thermal state. Input still lands on the next admitted frame.

`host::Config::crashReport` names a file. A fatal signal (`SIGSEGV`, `SIGBUS`, `SIGILL`, `SIGFPE`, `SIGABRT`, `SIGTRAP`) writes the signal and the stack to that file, one frame per line as `#n 0x<pc> <module> +0x<offset>`. Handlers installed earlier, such as a crash SDK, still run afterwards. On the next launch the host passes the report to `crashSink` and deletes the file; send it wherever your app sends field data. Per-frame telemetry goes the same way, through `TelemetrySink`. There are no signals on the web, so capture is off there.

Glim links statically, so its code is symbolized from the app's own symbols: upload the app's dSYM or unstripped `.so` with the app's usual pipeline. The SDK bundles keep function names and drop only debug info. To symbolize a report by hand, use `llvm-symbolizer --obj=<module> <offset>` on Linux and Android, or `atos -o <module> -l 0x100000000 <0x100000000 + offset>` on Apple platforms.
