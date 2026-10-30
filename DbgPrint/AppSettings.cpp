#include "pch.h"
#include "AppSettings.h"

namespace {
	std::wstring Trim(std::wstring const& text, PCWSTR chars = L" \t\r\n") {
		auto start = text.find_first_not_of(chars);
		if (start == std::wstring::npos)
			return L"";
		return text.substr(start, text.find_last_not_of(chars) - start + 1);
	}
}

std::wstring AppSettings::BuildSymbolSearchPath() const {
	return BuildSymbolSearchPath(SymbolServerEnabled(), SymbolServerUrl(), SymbolCache(), SymbolExtraPaths(), SymbolUseEnvPath());
}

std::wstring AppSettings::BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
	std::wstring const& extra, bool useEnv) {
	std::wstring path;
	auto add = [&](std::wstring const& part) {
		if (part.empty())
			return;
		if (!path.empty())
			path += L";";
		path += part;
	};

	// local locations first, so the server is only consulted when they have nothing
	add(Trim(extra, L" \t\r\n;"));
	if (useEnv) {
		WCHAR env[4096];
		auto len = ::GetEnvironmentVariable(L"_NT_SYMBOL_PATH", env, _countof(env));
		if (len > 0 && len < _countof(env))
			add(Trim(env, L" \t\r\n;"));
	}
	auto server = Trim(url);
	if (useServer && !server.empty()) {
		auto dir = Trim(cache);
		if (!dir.empty()) {
			WCHAR expanded[MAX_PATH];
			auto len = ::ExpandEnvironmentStrings(dir.c_str(), expanded, _countof(expanded));
			if (len > 0 && len <= _countof(expanded))
				dir = expanded;
		}
		add(dir.empty() ? L"srv*" + server : L"srv*" + dir + L"*" + server);
	}
	return path;
}

std::wstring AppSettings::FormatHighlights(std::vector<HighlightItem> const& items) {
	std::wstring text;
	WCHAR colors[32];
	for (auto& item : items) {
		swprintf_s(colors, L"%06X|%06X|", item.Light, item.Dark);
		text += colors;
		text += item.Text;
		text += L"\n";
	}
	return text;
}

std::vector<HighlightItem> AppSettings::ParseHighlights(std::wstring const& text) {
	std::vector<HighlightItem> items;
	size_t start = 0;
	while (start < text.size()) {
		auto end = text.find(L'\n', start);
		if (end == std::wstring::npos)
			end = text.size();
		auto line = text.substr(start, end - start);
		start = end + 1;
		if (!line.empty() && line.back() == L'\r')	// a file saved with CRLF line breaks
			line.pop_back();

		//
		// the text is everything after the second '|', so it may contain '|' itself
		//
		auto bar1 = line.find(L'|');
		auto bar2 = bar1 == std::wstring::npos ? std::wstring::npos : line.find(L'|', bar1 + 1);
		if (bar2 == std::wstring::npos)
			continue;
		HighlightItem item;
		item.Light = (COLORREF)wcstoul(line.substr(0, bar1).c_str(), nullptr, 16);
		item.Dark = (COLORREF)wcstoul(line.substr(bar1 + 1, bar2 - bar1 - 1).c_str(), nullptr, 16);
		item.Text = line.substr(bar2 + 1);
		if (!item.Text.empty())
			items.push_back(std::move(item));
	}
	return items;
}
