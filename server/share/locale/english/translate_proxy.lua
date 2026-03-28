-- translate_proxy.lua
-- Dynamic per-player translation system using metatable proxy
-- Loaded AFTER translate.lua, all translate_*.lua, locale.lua
--
-- At this point:
--   __translations["en"], ["de"], ["ro"], etc. = full gameforge tables per language
--   gameforge = EN plain table (original)
--   locale = EN plain table (set by locale.lua from gameforge)

if not __translations then
	__translations = {}
end

-- Store EN as fallback (already loaded as gameforge)
if not __translations["en"] then
	__translations["en"] = gameforge
end

-- Save original locale table (contains config numbers + EN strings)
__locale_original = locale

-- Safe language getter: returns "en" during server init (no player context)
local function get_current_language()
	local ok, lang = pcall(pc.get_language)
	if ok and lang and lang ~= "" then
		return lang
	end
	return "en"
end

-- Helper: navigate a nested table by key path
local function navigate(t, path)
	for _, k in ipairs(path) do
		if type(t) ~= "table" then return nil end
		t = t[k]
		if t == nil then return nil end
	end
	return t
end

-- Deep proxy: resolves per-player language at each table access level
local function make_proxy(key_path)
	local proxy = {}
	local mt = {
		__index = function(self, key)
			local lang = get_current_language()

			-- Try player's language first
			local t = navigate(__translations[lang], key_path)
			if t then
				local val = t[key]
				if val ~= nil then
					if type(val) == "table" then
						local new_path = {}
						for i, k in ipairs(key_path) do new_path[i] = k end
						table.insert(new_path, key)
						return make_proxy(new_path)
					end
					return val
				end
			end

			-- Fallback to EN
			if lang ~= "en" then
				t = navigate(__translations["en"], key_path)
				if t then
					local val = t[key]
					if val ~= nil then
						if type(val) == "table" then
							local new_path = {}
							for i, k in ipairs(key_path) do new_path[i] = k end
							table.insert(new_path, key)
							return make_proxy(new_path)
						end
						return val
					end
				end
			end

			return nil
		end,

		__newindex = function(self, key, value)
			-- Writes go to EN table
			local t = navigate(__translations["en"], key_path)
			if t then
				t[key] = value
			end
		end
	}
	return setmetatable(proxy, mt)
end

-- Replace gameforge with root proxy
gameforge = make_proxy({})

-- Replace locale with a proxy that:
--   - returns config values (numbers, colors) from original locale
--   - returns translated strings dynamically via gameforge.locale
locale = setmetatable({}, {
	__index = function(self, key)
		-- First check original locale for non-string values (numbers, colors, config)
		local orig = __locale_original[key]
		if orig ~= nil and type(orig) ~= "string" then
			-- Sub-tables (like locale.bookworm) need special handling:
			-- they mix config numbers with translated strings
			if type(orig) == "table" then
				return setmetatable({}, {
					__index = function(self2, subkey)
						local subval = orig[subkey]
						-- Numbers and non-strings: return from original
						if subval ~= nil and type(subval) ~= "string" then
							return subval
						end
						-- Strings: resolve dynamically from gameforge.locale.key.subkey
						local gf_sub = gameforge.locale
						if gf_sub then
							local npc = gf_sub[key]
							if npc then return npc[subkey] end
						end
						-- Fallback to original
						return subval
					end
				})
			end
			return orig
		end

		-- Strings: resolve dynamically from gameforge.locale
		local gf_locale = gameforge.locale
		if gf_locale then
			local val = gf_locale[key]
			if val ~= nil then return val end
		end

		-- Final fallback to original
		return orig
	end,

	__newindex = function(self, key, value)
		__locale_original[key] = value
	end
})
