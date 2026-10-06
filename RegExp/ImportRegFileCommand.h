#pragma once

#include "AppCommandBase.h"
#include "RegExportImport.h"

struct ImportRegFileCommand : AppCommandBase<ImportRegFileCommand> {
	ImportRegFileCommand(PCWSTR fileName, std::vector<RegFileKey> keys, AppCommandCallback<ImportRegFileCommand> cb = nullptr);

	bool Execute() override;
	bool Undo() override;

	// failures of the last Execute; like RegEdit, the import continues past them
	std::vector<CString> const& GetErrors() const;

private:
	enum class UndoType {
		RestoreValue,
		DeleteCreatedKey,
		RestoreDeletedKey,
	};

	struct UndoEntry {
		UndoType Type;
		CString Path;
		CString Name;
		DWORD ValueType{ REG_NONE };
		std::vector<BYTE> Data;
		bool Existed{ false };
	};

	void ImportKey(RegFileKey const& key);
	void DeleteKey(CString const& path);
	void AddError(CString const& path, PCWSTR name, DWORD error);

	std::vector<RegFileKey> m_Keys;
	std::vector<UndoEntry> m_UndoLog;
	std::vector<CString> m_Errors;
};
