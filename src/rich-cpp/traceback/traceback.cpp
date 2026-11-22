#include "Traceback.h"

#include <rich_core.h>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <psapi.h>
#  include <dbghelp.h>
#  pragma comment(lib, "dbghelp.lib")
#endif

namespace rich::traceback {

namespace {
#ifdef _WIN32
// (base, name, path) for every loaded module. Built once; K32EnumProcessModules
// returns base addresses directly (kernel32/psapi — no DbgHelp needed, and
// unlike SymGetModuleInfo64 it works on the local process).
struct ModEnt {
    uintptr_t base;
    std::string name;
    std::string path;
};
const std::vector<ModEnt>& module_table() {
    static const std::vector<ModEnt> table = []() {
        std::vector<ModEnt> t;
        HMODULE mods[512];
        DWORD needed = 0;
        HANDLE hProc = GetCurrentProcess();
        if (K32EnumProcessModules(hProc, mods, sizeof(mods), &needed) != 0) {
            const DWORD count = needed / sizeof(HMODULE);
            t.reserve(count);
            for (DWORD i = 0; i < count; ++i) {
                ModEnt m;
                m.base = reinterpret_cast<uintptr_t>(mods[i]);
                char name[MAX_PATH] = {0};
                K32GetModuleBaseNameA(hProc, mods[i], name, MAX_PATH);
                m.name = name;
                char path[MAX_PATH] = {0};
                if (K32GetModuleFileNameExA(hProc, mods[i], path, MAX_PATH) != 0)
                    m.path = path;
                t.push_back(std::move(m));
            }
        }
        return t;
    }();
    return table;
}

// One-time DbgHelp setup, for symbol names only. Local PDBs next to the
// modules — no symbol server, so offline-safe and fast. Failure is fine:
// frames then show module+offset without a symbol. Note DbgHelp does not
// auto-register the modules of a live process: each one must be loaded
// explicitly (SymLoadModule64) before SymFromAddr will resolve.
bool symbols_ready() {
    static const bool ok = []() {
        HANDLE hProc = GetCurrentProcess();
        char file[MAX_PATH] = {0};
        std::string dir;
        if (GetModuleFileNameA(nullptr, file, MAX_PATH) != 0) {
            dir = file;
            const size_t s = dir.find_last_of("/\\");
            if (s != std::string::npos) dir.erase(s);
        }
        const char* search = dir.empty() ? nullptr : dir.c_str();
        if (SymInitialize(hProc, search, FALSE) == 0) return false;
        for (const auto& m : module_table())
            if (!m.path.empty())
                SymLoadModule64(hProc, nullptr, m.path.c_str(), m.name.c_str(), m.base, 0);
        return true;
    }();
    return ok;
}

void resolve_frame(uintptr_t addr, bool have_symbols, Frame& f) {
    f.address = addr;
    // Module = the loaded image with the greatest base at or below the
    // address (module images never overlap; low-address stubs like
    // compatibility shims are handled by the max-base match).
    uintptr_t best = 0;
    const std::string* best_name = nullptr;
    for (const auto& m : module_table()) {
        if (m.base <= addr && m.base > best) {
            best = m.base;
            best_name = &m.name;
        }
    }
    if (best_name != nullptr) {
        f.module = *best_name;
        f.module_offset = static_cast<long long>(addr - best);
    }
    if (have_symbols) {
        SYMBOL_INFO sym{};
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLen = sizeof(sym.Name);
        if (SymFromAddr(GetCurrentProcess(), addr, nullptr, &sym) != 0)
            f.symbol = sym.Name;
    }
}
#endif // _WIN32
} // namespace

std::vector<Frame> capture_stack(size_t max_frames, size_t skip) {
    std::vector<Frame> frames;
#ifdef _WIN32
    if (max_frames == 0) return frames;
    std::vector<void*> addrs(max_frames);
    const DWORD n =
        CaptureStackBackTrace(static_cast<DWORD>(skip), static_cast<DWORD>(max_frames),
                              addrs.data(), nullptr);
    const bool have_symbols = symbols_ready();
    frames.reserve(n);
    for (DWORD i = 0; i < n; ++i) {
        Frame f;
        resolve_frame(reinterpret_cast<uintptr_t>(addrs[i]), have_symbols, f);
        frames.push_back(std::move(f));
    }
#endif // _WIN32
    return frames;
}

void print_current_trace() {
    const auto frames = capture_stack(32, 2); // hide capture_stack + this frame
    rich::Console c(std::cout);
    c.print("[bold]Traceback (most recent call last):[/bold]");
    if (frames.empty()) {
        c.print("  [dim](no frames captured)[/dim]");
        return;
    }
    for (size_t i = 0; i < frames.size(); ++i) {
        const Frame& f = frames[i];
        std::ostringstream line;
        line << "  #" << std::dec << i << "  ";
        if (f.symbol.empty()) {
            line << (f.module.empty() ? "?" : f.module);
        } else {
            line << (f.module.empty() ? f.symbol : f.module + "!" + f.symbol);
        }
        if (f.module_offset != 0) {
            line << " (+0x" << std::hex << f.module_offset << ")";
        }
        line << " @ 0x" << std::hex << f.address;
        c.print(line.str());
    }
}

} // namespace rich::traceback
