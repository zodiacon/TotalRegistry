#pragma once

const CString DeletedPathBackup(L"SOFTWARE\\ScorpioSoftware\\RegExp\\__Deleted__\\");

struct AppCommand abstract {
	explicit AppCommand(const CString& name) : m_CmdName(name) {}

	virtual CString GetCommandName() const {
		return m_CmdName;
	}

	void SetCommandName(PCWSTR name) {
		m_CmdName = name;
	}

	virtual ~AppCommand() = default;
	virtual bool Execute() = 0;
	virtual bool Undo() = 0;

private:
	CString m_CmdName;
};

template<typename TCommand>
using AppCommandCallback = std::function<bool(TCommand&, bool)>;

template<typename T>
struct AppCommandBase abstract : AppCommand {
	explicit AppCommandBase(const CString& name, AppCommandCallback<T> cb = nullptr) : AppCommand(name), m_Callback(cb) {}

	void SetCallback(AppCommandCallback<T> cb) {
		m_Callback = cb;
	}

protected:
	virtual bool InvokeCallback(bool execute) {
		if (m_Callback)
			return m_Callback(static_cast<T&>(*this), execute);
		return true;
	}

private:
	AppCommandCallback<T> m_Callback;
};

template<typename T>
struct RegAppCommandBase : AppCommandBase<T> {
	RegAppCommandBase(const CString& cmdname, PCWSTR path, PCWSTR name, AppCommandCallback<T> cb = nullptr) 
		: AppCommandBase<T>(cmdname, cb), m_Path(path), m_Name(name) {}

	const CString& GetPath() const {
		return m_Path;
	}
	const CString& GetName() const {
		return m_Name;
	}

protected:
	CString m_Path, m_Name;
};

struct AppCommandList final : AppCommandBase<AppCommandList> {
	AppCommandList(PCWSTR name = nullptr, AppCommandCallback<AppCommandList> cb = nullptr) : AppCommandBase(name, cb) {}

	void AddCommand(std::shared_ptr<AppCommand> command);

	std::shared_ptr<AppCommand> GetCommand(size_t i) const;
	int GetCount() const;

	bool Execute() override;
	bool Undo() override;

private:
	std::vector<std::shared_ptr<AppCommand>> m_Commands;
};
