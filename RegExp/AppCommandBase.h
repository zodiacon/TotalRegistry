#pragma once

const CString DeletedPathBackup(L"HKEY_CURRENT_USER\\ScorpioSoftware\\RegExp\\__Deleted__");

struct AppCommand abstract {
	explicit AppCommand(const CString& name) : _cmdname(name) {}

	const CString& GetCommandName() const {
		return _cmdname;
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

struct AppCommandList final : AppCommand {
	AppCommandList();

	void AddCommand(std::shared_ptr<AppCommand> command);

	bool Execute() override;
	bool Undo() override;

private:
	std::vector<std::shared_ptr<AppCommand>> _commands;
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

