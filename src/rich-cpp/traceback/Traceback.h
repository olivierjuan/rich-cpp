#pragma once
// Native stack capture + rich-styled printing (port of the idea behind
// rich.traceback, adapted to C++: Win32 CaptureStackBackTrace + DbgHelp
// symbol resolution). Zero third-party dependencies; on platforms without
// DbgHelp (or without PDBs) frames degrade to module+offset. Never crashes
// and never fails: unresolved fields are left empty/zero.
#include <rich_export.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rich::traceback {

// One stack frame, most recent first.
struct Frame {
    uintptr_t address{0};
    std::string module;    // e.g. "rich_core.dll" ("" if unknown)
    std::string symbol;    // e.g. "rich::Console::print" ("" if unresolved)
    long long module_offset{0}; // address - module base (0 if unknown)
};

// Capture up to `max_frames` native frames, dropping the top `skip` frames
// (frame 0 is the caller of capture_stack itself). Symbol resolution is
// best-effort (DbgHelp, local PDBs only, no symbol server); frames that
// cannot be resolved keep an empty `symbol` (and `module` if the module
// base is unknown). Returns an empty vector on non-Windows platforms.
RICH_EXPORT std::vector<Frame> capture_stack(size_t max_frames = 32,
                                             size_t skip = 1);

// Capture and print a rich-styled trace to std::cout:
//   Traceback (most recent call last):
//     #0  module!symbol (+0x1a2b) @ 0x7ff600000000
// The capture frame and this function's own frame are hidden.
RICH_EXPORT void print_current_trace();

} // namespace rich::traceback
