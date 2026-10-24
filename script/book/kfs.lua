-- ██╗  ██╗██████╗  ██████╗ ███████╗
-- ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
-- █████╔╝ ██║  ██║██║   ██║███████╗
-- ██╔═██╗ ██║  ██║██║   ██║╚════██║
-- ██║  ██╗██████╔╝╚██████╔╝███████║
-- ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
-- ---------------------------------
--   kfs.lua — the book's block classes, for typst
--
--   .kfs-page        a full package page: it starts a new PDF page
--   .kfs-card        a compact card: kfs.typ's #kfs-card, several a page
--   .kfs-note        a callout box: kfs.typ's #kfs-note
--   .kfs-list        paragraphs listing names: kfs.typ's #kfs-list, unjustified
--   .kfs-html-only   dropped: file lists, the contents page, navigation
--
-- HTML keeps every div as it is and styles it from kfs.css, so outside typst
-- this filter touches only kernel option names (below). The typst writer would otherwise turn each div
-- into a bare #block and lose the class, which is the only thing that says
-- a card from a page.
-- ---------------------------------

-- In HTML, a kernel option's name may break after each underscore, so a
-- long one wraps inside its table column instead of in mid-word or past it.
if FORMAT:match('html') then
  return { {
    Code = function(el)
      if el.text:match('^CONFIG_[%w_]+$') then
        local t = el.text:gsub('_', '_<wbr>')
        return pandoc.RawInline('html', '<code>' .. t .. '</code>')
      end
      return nil
    end,
  } }
end

if FORMAT ~= 'typst' then
  return {}
end

local function raw(s)
  return pandoc.RawBlock('typst', s)
end

local wrap = { ['kfs-card'] = true, ['kfs-note'] = true, ['kfs-list'] = true }

function Div(el)
  if el.classes:includes('kfs-html-only') then
    return {}
  end
  if el.classes:includes('kfs-page') then
    local out = { raw('#pagebreak(weak: true)') }
    for _, b in ipairs(el.content) do
      table.insert(out, b)
    end
    return out
  end
  for _, c in ipairs(el.classes) do
    if wrap[c] then
      local out = { raw('#' .. c .. '[') }
      for _, b in ipairs(el.content) do
        table.insert(out, b)
      end
      table.insert(out, raw(']'))
      return out
    end
  end
  return nil
end
