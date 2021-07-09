#pragma once

struct AppSettings;

struct IMainFrame abstract {
	virtual AppSettings& GetSettings() = 0;
	virtual void OnFindStart() = 0;
	virtual void OnFindNext(PCWSTR path, PCWSTR name, void* data) = 0;
	virtual void OnFindEnd(bool cancelled) = 0;
};
