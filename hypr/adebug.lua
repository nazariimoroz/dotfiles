return {
	notify = function(...)
		local parts = {}
		for i = 1, select("#", ...) do
			parts[i] = tostring(select(i, ...))
		end
		hl.notification.create({ text = table.concat(parts, " "), timeout = 4000, icon = "info" })
	end,
}
