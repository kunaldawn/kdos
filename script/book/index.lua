-- ██╗  ██╗██████╗  ██████╗ ███████╗
-- ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
-- █████╔╝ ██║  ██║██║   ██║███████╗
-- ██╔═██╗ ██║  ██║██║   ██║╚════██║
-- ██║  ██╗██████╔╝╚██████╔╝███████║
-- ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
-- ---------------------------------
--   index.lua — [term]{.idx} spans as index entries, for typst and HTML
--
-- A span with class idx is an index entry for its text, or for its `term`
-- attribute when it has one (an empty span then marks a place without
-- printing anything). For typst it becomes `#kfs-idx("term")`, which kfs.typ
-- turns into a metadata element its index page queries; for HTML it gets an
-- id `idx-N`, numbered in document order, and a data-term attribute, which
-- kfsrender.py collects into genindex.html. The span's own text is kept either
-- way.
-- ---------------------------------

local n = 0

local function typst_string(s)
  return '"' .. s:gsub('\\', '\\\\'):gsub('"', '\\"') .. '"'
end

function Span(el)
  if not el.classes:includes('idx') then
    return nil
  end
  local term = el.attributes['term'] or pandoc.utils.stringify(el.content)
  if term == '' then
    return el.content
  end
  n = n + 1
  if FORMAT == 'typst' then
    local out = { pandoc.RawInline('typst', '#kfs-idx(' .. typst_string(term) .. ')') }
    for _, i in ipairs(el.content) do
      table.insert(out, i)
    end
    return out
  end
  if FORMAT:match('html') then
    el.identifier = 'idx-' .. n
    el.attributes['term'] = nil
    el.attributes['data-term'] = term
    return el
  end
  return el.content
end
