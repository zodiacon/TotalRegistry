#pragma once

const CString DeletedPathBackup(L"SOFTWARE\\ScorpioSoftware\\RegExp\\__Deleted__\\");

struct AppCommand abstract {
	explicit AppCommand(const CString& name) : _cmdname(name) {}

	virtual CString GetCommandName() const {
		return _cmdname;
	}

	void SetCommandName(PCWSTR name) {
		_cmdname = name;
	}

	virtual ~AppCommand() = default;
	virtual bool Execute() = 0;
	virtual bool Undo() = 0;

private:
	CString _cmdname;
};

template<typename TCommand>
using AppCommandCallback = std::function<bool(TCommand&, bool)>;

template<typename T>
struct AppCommandBase abstract : AppCommand {
	explicit AppCommandBase(const CString& name, AppCommandCallback<T> cb = nullptr) : AppCommand(name), _cb(cb) {}

	void SetCallback(AppCommandCallback<T> cb) {
		_cb = cb;
	}

protected:
	virtual bool InvokeCallback(bool execute) {
		if (_cb)
			return _cb(static_cast<T&>(*this), execute);
		return true;
	}

private:
	AppCommandCallback<T> _cb;
};

template<typename T>
struct RegAppCommandBase : AppCommandBase<T> {
	RegAppCommandBase(const CString& cmdname, PCWSTR path, PCWSTR name, AppCommandCallback<T> cb = nullptr) 
		: AppCommandBase<T>(cmdname, cb), _path(path), _name(name) {}

	const CString& GetPath() const {
		return _path;
	}
	const CString& GetName() const {
		return _name;
	}

protected:
	CString _path, _name;
};

struct AppCommandList final : AppCommandBase<AppCommandList> {
	AppCommandList(PCWSTR name = nullptr, AppCommandCallback<AppCommandList> cb = nullptr) : AppCommandBase(name, cb) {}

	void AddCommand(std::shared_ptr<AppCommand> command);

	std::shared_ptr<AppCommand> GetCommand(size_t i) const;
	int GetCount() const;

	bool Execute() override;
	bool Undo() override;

private:
	std::vector<std::shared_ptr<AppCommand>> _commands;
};
