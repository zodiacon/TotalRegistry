#include "pch.h"
#include "AppCommandBase.h"

void AppCommandList::AddCommand(std::shared_ptr<AppCommand> command) {
	_commands.push_back(command);
}

std::shared_ptr<AppCommand> AppCommandList::GetCommand(size_t i) const {
	return i < _commands.size() ? _commands[i] : nullptr;
}

int AppCommandList::GetCount() const {
	return static_cast<int>(_commands.size());
}

bool AppCommandList::Execute() {
	for (auto& cmd : _commands)
		if (!cmd->Execute())
			return false;
	return InvokeCallback(true);
}

bool AppCommandList::Undo() {
	for (int i = (int)_commands.size() - 1; i >= 0; --i)
		if (!_commands[i]->Undo())
			return false;

	return InvokeCallback(false);
}
