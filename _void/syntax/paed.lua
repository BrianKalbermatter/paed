-- syntax/paed.lua
-- Corre cuando Neovim setea syntax=paed (igual que syntax/paed.vim).
-- Lee data/sintaxis.json subiendo desde el archivo actual — sin hardcodear nada.

if vim.b.current_syntax then return end

-- Treesitter interfiere con la sintaxis tradicional — lo apagamos para .paed
pcall(vim.cmd, "TSBufDisable highlight")

-- ── Buscar data/sintaxis.json relativo al plugin ─────────────────────────────
local function find_json()
    local plugin_dir = vim.fn.fnamemodify(
        debug.getinfo(1, "S").source:sub(2), ":h:h"
    )
    return plugin_dir .. "/data/sintaxis.json"
end

-- ── Colores del pseudocodigo → Vim (gruvbox) ─────────────────────────────────
local COLOR = {
    ["azul"]         = { cterm = 109, gui = "#83a598" },
    ["naranja"]      = { cterm = 208, gui = "#fe8019" },
    ["dorado"]       = { cterm = 214, gui = "#d79921" },
    ["violeta"]      = { cterm = 132, gui = "#b16286" },
    ["verde-teal"]   = { cterm = 108, gui = "#8ec07c" },
    ["azul-claro"]   = { cterm = 109, gui = "#83a598" },
    ["verde-lima"]   = { cterm = 142, gui = "#b8bb26" },
    ["amarillo"]     = { cterm = 214, gui = "#fabd2f" },
    ["aqua"]         = { cterm = 108, gui = "#8ec07c" },
    ["gris"]         = { cterm = 244, gui = "#928374" },
    ["blanco-crema"] = { cterm = 223, gui = "#ebdbb2" },
    ["rosa-lila"]    = { cterm = 175, gui = "#d3869b" },
}

local BOLD   = { condicionales = true, bucles = true, estructura = true }
local ITALIC = { comentario = true }

local function grupo(nombre)
    local r = nombre:gsub("[-_](%a)", function(c) return c:upper() end)
    return "paed" .. r:sub(1,1):upper() .. r:sub(2)
end

local function is_word(s)
    return s:match("^[A-Za-z_]+$") ~= nil
end

-- ── Cargar JSON ───────────────────────────────────────────────────────────────
local path = find_json()
if not path then
    vim.notify("[paed] sintaxis.json no encontrado", vim.log.levels.WARN)
    return
end

local ok, data = pcall(function()
    return vim.fn.json_decode(table.concat(vim.fn.readfile(path), "\n"))
end)
if not ok or not data.categorias then
    vim.notify("[paed] error leyendo sintaxis.json", vim.log.levels.ERROR)
    return
end

-- ── Aplicar sintaxis ──────────────────────────────────────────────────────────
for _, cat in ipairs(data.categorias) do
    local nombre = cat.nombre
    local grp    = grupo(nombre)
    local col    = COLOR[cat.color] or { cterm = 223, gui = "#ebdbb2" }

    if cat.palabras then
        local kws = {}
        for _, p in ipairs(cat.palabras) do
            if is_word(p) then kws[#kws + 1] = p end
        end
        if #kws > 0 then
            vim.cmd("syntax keyword " .. grp .. " " .. table.concat(kws, " "))
        end
    elseif cat.patron then
        local p = cat.patron
        if     p == "//"     then vim.cmd('syntax match '  .. grp .. ' "//.*$"')
        elseif p == '".*"'   then vim.cmd('syntax region ' .. grp .. ' start=/"/ end=/"/')
        elseif p == "'.*'"   then vim.cmd("syntax region " .. grp .. " start=/'/ end=/'/")
        elseif p:find("%[0%-9%]") then
            vim.cmd('syntax match ' .. grp .. ' "\\<[0-9]\\+\\(\\.[0-9]\\+\\)\\?\\>"')
        end
    end

    local extra = ""
    if BOLD[nombre]   then extra = " cterm=bold   gui=bold"   end
    if ITALIC[nombre] then extra = " cterm=italic gui=italic" end

    vim.cmd(string.format(
        "highlight default %s ctermfg=%d guifg=%s%s",
        grp, col.cterm, col.gui, extra
    ))
end

vim.b.current_syntax = "paed"
