vim.cmd("highlight clear")

if vim.fn.exists("syntax_on") == 1 then
  vim.cmd("syntax reset")
end

vim.g.colors_name = "vscode"
vim.o.background = "dark"

local c = {
  bg = "#1e1e1e",
  fg = "#d4d4d4",
  line = "#2a2d2e",
  selection = "#264f78",
  inactive = "#3c3c3c",
  border = "#454545",
  comment = "#6a9955",
  string = "#ce9178",
  keyword = "#569cd6",
  type = "#4ec9b0",
  func = "#dcdcaa",
  variable = "#9cdcfe",
  constant = "#4fc1ff",
  number = "#b5cea8",
  operator = "#d4d4d4",
  purple = "#c586c0",
  yellow = "#d7ba7d",
  red = "#f44747",
  orange = "#ce9178",
  blue = "#3794ff",
  green = "#b5cea8",
  dim = "#858585",
  gutter = "#858585",
  popup = "#252526",
}

local function hl(group, opts)
  vim.api.nvim_set_hl(0, group, opts)
end

hl("Normal", { fg = c.fg, bg = c.bg })
hl("NormalFloat", { fg = c.fg, bg = c.popup })
hl("FloatBorder", { fg = c.border, bg = c.popup })
hl("Cursor", { fg = c.bg, bg = c.fg })
hl("CursorLine", { bg = c.line })
hl("CursorLineNr", { fg = c.fg, bold = true })
hl("LineNr", { fg = c.gutter })
hl("SignColumn", { bg = c.bg })
hl("ColorColumn", { bg = c.line })
hl("Visual", { bg = c.selection })
hl("Search", { fg = c.bg, bg = c.yellow })
hl("IncSearch", { fg = c.bg, bg = c.orange })
hl("MatchParen", { fg = c.fg, bg = c.border })
hl("Pmenu", { fg = c.fg, bg = c.popup })
hl("PmenuSel", { fg = c.fg, bg = "#04395e" })
hl("PmenuSbar", { bg = c.inactive })
hl("PmenuThumb", { bg = c.gutter })
hl("StatusLine", { fg = c.fg, bg = c.inactive })
hl("StatusLineNC", { fg = c.dim, bg = c.inactive })
hl("VertSplit", { fg = c.border })
hl("WinSeparator", { fg = c.border })
hl("TabLine", { fg = c.dim, bg = c.inactive })
hl("TabLineSel", { fg = c.fg, bg = c.bg })
hl("TabLineFill", { bg = c.inactive })

hl("Comment", { fg = c.comment, italic = true })
hl("Constant", { fg = c.constant })
hl("String", { fg = c.string })
hl("Character", { fg = c.string })
hl("Number", { fg = c.number })
hl("Boolean", { fg = c.keyword })
hl("Float", { fg = c.number })
hl("Identifier", { fg = c.variable })
hl("Function", { fg = c.func })
hl("Statement", { fg = c.keyword })
hl("Conditional", { fg = c.keyword })
hl("Repeat", { fg = c.keyword })
hl("Label", { fg = c.keyword })
hl("Operator", { fg = c.operator })
hl("Keyword", { fg = c.keyword })
hl("Exception", { fg = c.purple })
hl("PreProc", { fg = c.purple })
hl("Include", { fg = c.purple })
hl("Define", { fg = c.purple })
hl("Macro", { fg = c.purple })
hl("PreCondit", { fg = c.purple })
hl("Type", { fg = c.type })
hl("StorageClass", { fg = c.keyword })
hl("Structure", { fg = c.type })
hl("Typedef", { fg = c.type })
hl("Special", { fg = c.yellow })
hl("SpecialChar", { fg = c.yellow })
hl("Tag", { fg = c.keyword })
hl("Delimiter", { fg = c.fg })
hl("SpecialComment", { fg = c.comment, italic = true })
hl("Debug", { fg = c.red })
hl("Underlined", { fg = c.blue, underline = true })
hl("Ignore", { fg = c.dim })
hl("Error", { fg = c.red })
hl("Todo", { fg = c.bg, bg = c.yellow })

hl("DiagnosticError", { fg = c.red })
hl("DiagnosticWarn", { fg = "#ffcc02" })
hl("DiagnosticInfo", { fg = c.blue })
hl("DiagnosticHint", { fg = c.type })
hl("DiagnosticUnderlineError", { undercurl = true, sp = c.red })
hl("DiagnosticUnderlineWarn", { undercurl = true, sp = "#ffcc02" })
hl("DiagnosticUnderlineInfo", { undercurl = true, sp = c.blue })
hl("DiagnosticUnderlineHint", { undercurl = true, sp = c.type })
hl("DiagnosticVirtualTextError", { fg = c.red, bg = "#2d2020" })
hl("DiagnosticVirtualTextWarn", { fg = "#ffcc02", bg = "#2d2a1e" })
hl("DiagnosticVirtualTextInfo", { fg = c.blue, bg = "#1e2732" })
hl("DiagnosticVirtualTextHint", { fg = c.type, bg = "#1e2b29" })

hl("@comment", { link = "Comment" })
hl("@string", { link = "String" })
hl("@character", { link = "Character" })
hl("@number", { link = "Number" })
hl("@boolean", { link = "Boolean" })
hl("@constant", { link = "Constant" })
hl("@constant.builtin", { fg = c.constant })
hl("@variable", { fg = c.variable })
hl("@variable.builtin", { fg = c.keyword })
hl("@variable.parameter", { fg = c.variable })
hl("@property", { fg = c.variable })
hl("@field", { fg = c.variable })
hl("@function", { fg = c.func })
hl("@function.builtin", { fg = c.func })
hl("@function.call", { fg = c.func })
hl("@method", { fg = c.func })
hl("@method.call", { fg = c.func })
hl("@constructor", { fg = c.type })
hl("@keyword", { fg = c.keyword })
hl("@keyword.function", { fg = c.keyword })
hl("@keyword.operator", { fg = c.keyword })
hl("@operator", { fg = c.operator })
hl("@type", { fg = c.type })
hl("@type.builtin", { fg = c.keyword })
hl("@namespace", { fg = c.type })
hl("@module", { fg = c.type })
hl("@punctuation", { fg = c.fg })
hl("@punctuation.bracket", { fg = c.fg })
hl("@punctuation.delimiter", { fg = c.fg })
hl("@preproc", { fg = c.purple })
hl("@define", { fg = c.purple })
hl("@macro", { fg = c.purple })

hl("@lsp.type.class", { fg = c.type })
hl("@lsp.type.struct", { fg = c.type })
hl("@lsp.type.enum", { fg = c.type })
hl("@lsp.type.interface", { fg = c.type })
hl("@lsp.type.type", { fg = c.type })
hl("@lsp.type.typeParameter", { fg = c.type })
hl("@lsp.type.namespace", { fg = c.type })
hl("@lsp.type.function", { fg = c.func })
hl("@lsp.type.method", { fg = c.func })
hl("@lsp.type.macro", { fg = c.purple })
hl("@lsp.type.variable", { fg = c.variable })
hl("@lsp.type.parameter", { fg = c.variable })
hl("@lsp.type.property", { fg = c.variable })
hl("@lsp.type.enumMember", { fg = c.constant })
hl("@lsp.type.comment", { fg = c.comment, italic = true })
hl("@lsp.mod.deprecated", { fg = c.dim, strikethrough = true })

hl("cType", { fg = c.type })
hl("cStructure", { fg = c.type })
hl("cStorageClass", { fg = c.keyword })
hl("cStatement", { fg = c.keyword })
hl("cConditional", { fg = c.keyword })
hl("cRepeat", { fg = c.keyword })
hl("cOperator", { fg = c.keyword })
hl("cPreProc", { fg = c.purple })
hl("cInclude", { fg = c.purple })
hl("cDefine", { fg = c.purple })
hl("cString", { fg = c.string })
hl("cNumber", { fg = c.number })
hl("cFloat", { fg = c.number })
hl("cppType", { fg = c.type })
hl("cppStructure", { fg = c.type })
hl("cppStatement", { fg = c.keyword })
hl("cppModifier", { fg = c.keyword })
hl("cppStorageClass", { fg = c.keyword })
hl("cppExceptions", { fg = c.purple })
hl("cppOperator", { fg = c.keyword })
hl("cppBoolean", { fg = c.keyword })
