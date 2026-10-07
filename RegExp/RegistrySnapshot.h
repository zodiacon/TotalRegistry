#pragma once

#include <optional>

//
// a recording of a key tree, to compare with the live Registry or another snapshot later
// values keep their type, size, a hash of the data and its first bytes (enough to show what changed)
//
struct SnapshotValue {
	CString Name;
	DWORD Type{ REG_NONE };
	DWORD Size{ 0 };
	ULONGLONG Hash{ 0 };
	std::vector<BYTE> Preview;		// the first PreviewSize bytes

	static constexpr DWORD PreviewSize = 256;
	bool IsTruncated() const {
		return Preview.size() < Size;
	}
};

struct SnapshotKey {
	CString Path;		// relative to the root, empty for the root itself
	std::vector<SnapshotValue> Values;	// sorted by name, case-insensitive
};

enum class SnapshotChangeType {
	KeyAdded,
	KeyDeleted,
	ValueAdded,
	ValueDeleted,
	ValueChanged,
	NoAccess,		// a key that couldn't be read (previews only)
};

struct SnapshotChange {
	SnapshotChangeType Type;
	CString Key;		// full path
	CString Value;		// empty for key changes and default values
	std::optional<SnapshotValue> Old, New;
};

class RegistrySnapshot {
public:
	// progress gets the number of keys recorded so far; returning false cancels
	using ProgressCallback = std::function<bool(size_t keys)>;

	// records the tree under a key (standard, real or remote path); symbolic links are recorded, not followed
	bool Take(CString const& root, ProgressCallback progress = nullptr);
	bool Save(PCWSTR fileName) const;
	bool Load(PCWSTR fileName);

	CString const& GetRoot() const;
	FILETIME GetTime() const;
	std::vector<SnapshotKey> const& GetKeys() const;	// sorted by path, case-insensitive; the root first
	size_t GetValueCount() const;
	// keys that couldn't be opened while taking the snapshot
	size_t GetInaccessibleKeys() const;

	// what changed from before to after; both must be of the same root
	static std::vector<SnapshotChange> Compare(RegistrySnapshot const& before, RegistrySnapshot const& after);
	// readable data, e.g. for the comparison results
	static CString FormatData(SnapshotValue const& value);
	static CString GetChangeTypeName(SnapshotChangeType type);
	static bool IsKeyChange(SnapshotChangeType type);
	// the text of a change list column: change, key, value, old data, new data
	static CString GetChangeText(SnapshotChange const& change, int column);

private:
	CString m_Root;
	FILETIME m_Time{};
	std::vector<SnapshotKey> m_Keys;
	size_t m_Inaccessible{ 0 };
};
