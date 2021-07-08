#include "pch.h"
#include "AppCommandBase.h"

AppCommandList::AppCommandList() : AppCommand(L"") {
	_commands.reserve(4);
}

void AppCommandList::AddCommand(std::shared_ptr<AppCommand> command) {
	_commands.push_back(command);
}

bool AppCommandList::Execute() {
	for (auto& cmd : _commands)
		if (!cmd->Execute())
			return false;
	return true;
}

bool AppCommandList::Undo() {
	for (size_t i = _commands.size() - 1; i >= 0; --i)
		if (!_commands[i]->Undo())
			return false;
	return true;
}
