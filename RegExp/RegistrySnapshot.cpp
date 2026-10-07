#include "pch.h"
#include "RegistrySnapshot.h"
#include "Registry.h"
#include "ValueDecoder.h"
#include <wil\resource.h>

namespace {
	int CompareNoCase(CString const& a, CString const& b) {
		return ::CompareStringOrdinal(a, a.GetLength(), b, b.GetLength(), TRUE) - CSTR_EQUAL;
	}

	ULONGLONG Hash(BYTE const* data, DWORD size) {
		// FNV-1a: stable across runs and versions, as hashes are saved in files
		ULONGLONG hash = 14695981039346656037ULL;
		for (DWORD i = 0; i < size; i++) {
			hash ^= data[i];
			hash *= 1099511628211ULL;
		}
		return hash;
	}

	void RecordValues(HKEY hKey, SnapshotKey& key) {
		auto name = std::make_unique<WCHAR[]>(16384);
		std::vector<BYTE> data(4096);
		for (DWORD i = 0; ; i++) {
			DWORD nameLen = 16384, type, size = (DWORD)data.size();
			auto error = ::RegEnumValue(hKey, i, name.get(), &nameLen, nullptr, &type, data.data(), &size);
			if (error == ERROR_MORE_DATA) {
				data.resize(size);
				nameLen = 16384;
				error = ::RegEnumValue(hKey, i, name.get(), &nameLen, nullptr, &type, data.data(), &size);
			}
			if (error != ERROR_SUCCESS)
				break;

			SnapshotValue value;
			value.Name = name.get();
			value.Type = type;
			value.Size = size;
			value.Hash = Hash(data.data(), size);
			value.Preview.assign(data.data(), data.data() + std::min(size, SnapshotValue::PreviewSize));
			key.Values.push_back(std::move(value));
		}
		std::ranges::sort(key.Values, [](auto& a, auto& b) { return CompareNoCase(a.Name, b.Name) < 0; });
	}

	//
	// file format: a header, then the keys with their values; little endian, strings are a count and UTF-16 characters
	//
	const char Magic[8] = { 'T', 'R', 'S', 'N', 'A', 'P', 0, 0 };
	const DWORD FileVersion = 1;
	const DWORD MaxString = 1 << 20;

	class Writer {
	public:
		explicit Writer(HANDLE hFile) : m_hFile(hFile) {}
		~Writer() {
			Flush();
		}
		void Bytes(void const* p, size_t size) {
			m_Buffer.insert(m_Buffer.end(), (BYTE const*)p, (BYTE const*)p + size);
			if (m_Buffer.size() >= (1 << 20))
				Flush();
		}
		template<typename T>
		void Number(T value) {
			Bytes(&value, sizeof(value));
		}
		void String(CString const& text) {
			Number<DWORD>(text.GetLength());
			Bytes(text.GetString(), text.GetLength() * sizeof(WCHAR));
		}
		bool Flush() {
			DWORD written;
			if (!m_Buffer.empty() && (!::WriteFile(m_hFile, m_Buffer.data(), (DWORD)m_Buffer.size(), &written, nullptr) || written != m_Buffer.size()))
				m_Failed = true;
			m_Buffer.clear();
			return !m_Failed;
		}

	private:
		HANDLE m_hFile;
		std::vector<BYTE> m_Buffer;
		bool m_Failed{ false };
	};

	class Reader {
	public:
		explicit Reader(HANDLE hFile) : m_hFile(hFile), m_Buffer(1 << 20) {}
		bool Bytes(void* p, size_t size) {
			auto out = (BYTE*)p;
			while (size > 0) {
				if (m_Pos == m_End) {
					DWORD read;
					if (!::ReadFile(m_hFile, m_Buffer.data(), (DWORD)m_Buffer.size(), &read, nullptr) || read == 0)
						return false;
					m_Pos = 0;
					m_End = read;
				}
				auto count = std::min(size, m_End - m_Pos);
				memcpy(out, m_Buffer.data() + m_Pos, count);
				m_Pos += count;
				out += count;
				size -= count;
			}
			return true;
		}
		template<typename T>
		bool Number(T& value) {
			return Bytes(&value, sizeof(value));
		}
		bool String(CString& text) {
			DWORD length;
			if (!Number(length) || length > MaxString)
				return false;
			if (!Bytes(text.GetBufferSetLength(length), length * sizeof(WCHAR)))
				return false;
			text.ReleaseBuffer(length);
			return true;
		}

	private:
		HANDLE m_hFile;
		std::vector<BYTE> m_Buffer;
		size_t m_Pos{ 0 }, m_End{ 0 };
	};
}

bool RegistrySnapshot::Take(CString const& root, ProgressCallback progress) {
	m_Keys.clear();
	m_Inaccessible = 0;
	m_Root = root;
	m_Root.TrimRight(L'\\');
	::GetSystemTimeAsFileTime(&m_Time);

	auto rootKey = std::make_shared<RegistryKey>(Registry::OpenKey(m_Root, KEY_READ));
	if (!*rootKey)
		return false;

	// depth first; subkeys are opened when reached, through their (still open) parent
	struct Pending {
		std::shared_ptr<RegistryKey> Parent;
		CString Name;
		CString Path;
	};
	std::vector<Pending> pending;
	auto visit = [&](std::shared_ptr<RegistryKey> const& key, CString const& path) {
		SnapshotKey snapshotKey;
		snapshotKey.Path = path;
		RecordValues(key->Get(), snapshotKey);
		m_Keys.push_back(std::move(snapshotKey));

		std::vector<CString> names;
		Registry::EnumSubKeys(key->Get(), [&](auto name, const auto&) {
			names.push_back(name);
			return true;
			});
		// reversed, so they're visited in order
		for (auto it = names.rbegin(); it != names.rend(); ++it)
			pending.push_back({ key, *it, path.IsEmpty() ? *it : path + L"\\" + *it });
	};

	visit(rootKey, L"");
	while (!pending.empty()) {
		auto item = std::move(pending.back());
		pending.pop_back();

		HKEY hKey;
		if (ERROR_SUCCESS != ::RegOpenKeyEx(item.Parent->Get(), item.Name, REG_OPTION_OPEN_LINK, KEY_READ, &hKey)) {
			m_Inaccessible++;
			continue;
		}
		visit(std::make_shared<RegistryKey>(hKey), item.Path);

		if (progress && m_Keys.size() % 256 == 0 && !progress(m_Keys.size())) {
			m_Keys.clear();
			::SetLastError(ERROR_CANCELLED);
			return false;
		}
	}

	std::ranges::sort(m_Keys, [](auto& a, auto& b) { return CompareNoCase(a.Path, b.Path) < 0; });
	if (progress)
		progress(m_Keys.size());
	return true;
}

bool RegistrySnapshot::Save(PCWSTR fileName) const {
	wil::unique_hfile hFile(::CreateFile(fileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr));
	if (!hFile)
		return false;

	Writer w(hFile.get());
	w.Bytes(Magic, sizeof(Magic));
	w.Number(FileVersion);
	w.String(m_Root);
	w.Number(m_Time);
	w.Number<ULONGLONG>(m_Inaccessible);
	w.Number<ULONGLONG>(m_Keys.size());
	for (auto& key : m_Keys) {
		w.String(key.Path);
		w.Number<DWORD>((DWORD)key.Values.size());
		for (auto& value : key.Values) {
			w.String(value.Name);
			w.Number(value.Type);
			w.Number(value.Size);
			w.Number(value.Hash);
			w.Number<DWORD>((DWORD)value.Preview.size());
			w.Bytes(value.Preview.data(), value.Preview.size());
		}
	}
	if (!w.Flush()) {
		hFile.reset();
		::DeleteFile(fileName);
		return false;
	}
	return true;
}

bool RegistrySnapshot::Load(PCWSTR fileName) {
	wil::unique_hfile hFile(::CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (!hFile)
		return false;

	auto fail = [] {
		::SetLastError(ERROR_INVALID_DATA);
		return false;
	};
	Reader r(hFile.get());
	char magic[sizeof(Magic)];
	DWORD version;
	if (!r.Bytes(magic, sizeof(magic)) || memcmp(magic, Magic, sizeof(magic)) != 0 || !r.Number(version) || version != FileVersion)
		return fail();

	RegistrySnapshot snapshot;
	ULONGLONG inaccessible, keyCount;
	if (!r.String(snapshot.m_Root) || !r.Number(snapshot.m_Time) || !r.Number(inaccessible) || !r.Number(keyCount))
		return fail();
	snapshot.m_Inaccessible = (size_t)inaccessible;
	for (ULONGLONG k = 0; k < keyCount; k++) {
		SnapshotKey key;
		DWORD valueCount;
		if (!r.String(key.Path) || !r.Number(valueCount))
			return fail();
		for (DWORD v = 0; v < valueCount; v++) {
			SnapshotValue value;
			DWORD previewSize;
			if (!r.String(value.Name) || !r.Number(value.Type) || !r.Number(value.Size) || !r.Number(value.Hash) || !r.Number(previewSize)
				|| previewSize > SnapshotValue::PreviewSize || previewSize > value.Size)
				return fail();
			value.Preview.resize(previewSize);
			if (!r.Bytes(value.Preview.data(), previewSize))
				return fail();
			key.Values.push_back(std::move(value));
		}
		snapshot.m_Keys.push_back(std::move(key));
	}
	*this = std::move(snapshot);
	return true;
}

CString const& RegistrySnapshot::GetRoot() const {
	return m_Root;
}

FILETIME RegistrySnapshot::GetTime() const {
	return m_Time;
}

std::vector<SnapshotKey> const& RegistrySnapshot::GetKeys() const {
	return m_Keys;
}

size_t RegistrySnapshot::GetValueCount() const {
	size_t count = 0;
	for (auto& key : m_Keys)
		count += key.Values.size();
	return count;
}

size_t RegistrySnapshot::GetInaccessibleKeys() const {
	return m_Inaccessible;
}

std::vector<SnapshotChange> RegistrySnapshot::Compare(RegistrySnapshot const& before, RegistrySnapshot const& after) {
	std::vector<SnapshotChange> changes;
	auto fullPath = [&](CString const& path) {
		return path.IsEmpty() ? after.m_Root : after.m_Root + L"\\" + path;
	};
	auto addValues = [&](SnapshotKey const& key, SnapshotChangeType type) {
		for (auto& value : key.Values) {
			SnapshotChange change{ type, fullPath(key.Path), value.Name };
			(type == SnapshotChangeType::ValueAdded ? change.New : change.Old) = value;
			changes.push_back(std::move(change));
		}
	};

	// both are sorted, so walk them together
	auto& b = before.m_Keys;
	auto& a = after.m_Keys;
	size_t i = 0, j = 0;
	while (i < b.size() || j < a.size()) {
		int cmp = i == b.size() ? 1 : j == a.size() ? -1 : CompareNoCase(b[i].Path, a[j].Path);
		if (cmp < 0) {
			changes.push_back({ SnapshotChangeType::KeyDeleted, fullPath(b[i].Path) });
			addValues(b[i++], SnapshotChangeType::ValueDeleted);
		}
		else if (cmp > 0) {
			changes.push_back({ SnapshotChangeType::KeyAdded, fullPath(a[j].Path) });
			addValues(a[j++], SnapshotChangeType::ValueAdded);
		}
		else {
			auto& bv = b[i].Values;
			auto& av = a[j].Values;
			auto path = fullPath(a[j].Path);
			size_t x = 0, y = 0;
			while (x < bv.size() || y < av.size()) {
				int c = x == bv.size() ? 1 : y == av.size() ? -1 : CompareNoCase(bv[x].Name, av[y].Name);
				if (c < 0) {
					SnapshotChange change{ SnapshotChangeType::ValueDeleted, path, bv[x].Name };
					change.Old = bv[x++];
					changes.push_back(std::move(change));
				}
				else if (c > 0) {
					SnapshotChange change{ SnapshotChangeType::ValueAdded, path, av[y].Name };
					change.New = av[y++];
					changes.push_back(std::move(change));
				}
				else {
					auto& ov = bv[x++];
					auto& nv = av[y++];
					if (ov.Type != nv.Type || ov.Size != nv.Size || ov.Hash != nv.Hash) {
						SnapshotChange change{ SnapshotChangeType::ValueChanged, path, nv.Name };
						change.Old = ov;
						change.New = nv;
						changes.push_back(std::move(change));
					}
				}
			}
			i++;
			j++;
		}
	}
	return changes;
}

CString RegistrySnapshot::FormatData(SnapshotValue const& value) {
	auto& data = value.Preview;
	CString text;
	auto strings = [&](PCWSTR separator) {
		auto chars = (PCWSTR)data.data();
		auto count = data.size() / sizeof(WCHAR);
		size_t start = 0;
		for (size_t i = 0; i <= count; i++) {
			if (i == count || chars[i] == 0) {
				if (i == start)
					break;
				if (!text.IsEmpty())
					text += separator;
				text += CString(chars + start, int(i - start));
				start = i + 1;
			}
		}
	};

	switch (value.Type) {
		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_LINK:
			strings(L"");
			break;

		case REG_MULTI_SZ:
			strings(L" | ");
			break;

		case REG_DWORD:
		case REG_DWORD_BIG_ENDIAN:
			if (value.Size == sizeof(DWORD) && data.size() == sizeof(DWORD)) {
				DWORD number;
				memcpy(&number, data.data(), sizeof(number));
				return ValueDecoder::FormatNumber(value.Type == REG_DWORD ? number : _byteswap_ulong(number), sizeof(number), false);
			}
			[[fallthrough]];

		case REG_QWORD:
			if (value.Size == sizeof(ULONGLONG) && data.size() == sizeof(ULONGLONG)) {
				ULONGLONG number;
				memcpy(&number, data.data(), sizeof(number));
				return ValueDecoder::FormatNumber(number, sizeof(number), false);
			}
			[[fallthrough]];

		default:
			for (size_t i = 0; i < std::min<size_t>(data.size(), 64); i++)
				text.AppendFormat(L"%02X ", data[i]);
			text.TrimRight();
			if (value.Size > 64)
				text += L" ...";
			return text;
	}
	if (value.IsTruncated())
		text += L"...";
	return text;
}

CString RegistrySnapshot::GetChangeTypeName(SnapshotChangeType type) {
	switch (type) {
		case SnapshotChangeType::KeyAdded: return L"Key added";
		case SnapshotChangeType::KeyDeleted: return L"Key deleted";
		case SnapshotChangeType::ValueAdded: return L"Value added";
		case SnapshotChangeType::ValueDeleted: return L"Value deleted";
		case SnapshotChangeType::ValueChanged: return L"Value changed";
	}
	return L"";
}
