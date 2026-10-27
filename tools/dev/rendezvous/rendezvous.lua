-- rendezvous.lua: a sider module that loads the rendezvous hook DLL (rv.dll) and installs its hooks.
--
-- Installed by tools/dev/rendezvous/install_rendezvous.py (plan by default; --apply writes, --undo
-- reverses). Files it expects beside sider.ini:
--   rendezvous\rv.dll        the hook DLL
--   rendezvous\hooks.cfg     one `hook NAME VA LEN FIRST EVERY HEX` line per hooked entry
--   rendezvous\run\          a directory the DLL writes capture_N.txt / release_N / done_N into
-- If hooks.cfg is absent the module does nothing (it logs that and returns).
-- Needs sider's Lua runtime and LuaJIT FFI (lua.enabled = 1, luajit.ext.enabled = 1). Sider stays
-- enabled: it is the loader.
-- The sample rate and timeout are the owner's decision; the timeout here is the DLL's wait for a release.

-- sider's sandbox has no `require`: LuaJIT's ffi is the global `ffi` when luajit.ext.enabled = 1 (sider's own
-- modules use it that way, e.g. cpkdump.lua).
local ffi = ffi

local m = {}

-- 60 s: a capture takes as long as it needs (owner, 27-10-26; the bot tolerates the pause with
-- RIGBOT_ALLOW_FROZEN_FRAMES). The game is SIGSTOPped while memory is read, so the snapshot is consistent even if
-- this expires; it only bounds how long a missing watcher can park the hooked thread.
local TIMEOUT_MS = 60000

function m.init(ctx)
    if not ffi then
        log("rendezvous: LuaJIT ffi is not available (luajit.ext.enabled = 1 in sider.ini); not installed")
        return
    end
    ffi.cdef[[ int rv_init(const char *cfg_path, const char *run_dir, uint32_t timeout_ms); ]]
    local dir = ctx.sider_dir .. "rendezvous\\"   -- ctx.sider_dir ends with a path separator
    local cfg = dir .. "hooks.cfg"
    local f = io.open(cfg, "r")
    if f == nil then
        log("rendezvous: no hooks.cfg at " .. cfg .. "; not installed")
        return
    end
    f:close()
    -- Held on the module for the process's life: a collected ffi.load handle calls FreeLibrary, and the hooks call
    -- into the DLL (rv_init also pins it; this is the second guard).
    m.lib = ffi.load(dir .. "rv.dll")
    local n = m.lib.rv_init(cfg, dir .. "run\\", TIMEOUT_MS)
    log(string.format("rendezvous: %d hook(s) installed from %s", n, cfg))
end

return m
