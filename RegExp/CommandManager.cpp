#include "pch.h"
#include "CommandManager.h"
#include "AppCommandBase.h"

void CommandManager::Enable(bool enable) {
	_enabled = enable;
}

bool CommandManager::IsEnabled() const {
	return _enabled;
}

bool CommandManager::CanUndo() const {
	return !_undoList.empty();
}

bool CommandManager::CanRedo() const {
	return !_redoList.empty();
}

bool CommandManager::AddCommand(std::shared_ptr<AppCommand> command, bool execute) {
	if (execute)
		if (!command->Execute())
			return false;

	if (_enabled)
		return true;

	_undoList.push_back(command);
	_redoList.clear();
	return true;
}

bool CommandManager::Undo() {
	if (!CanUndo())
		return false;

	auto cmd = _undoList.back();
	auto success = cmd->Undo();
	if (success) {
		_redoList.push_back(cmd);
		_undoList.pop_back();
	}
	return success;
}

bool CommandManager::Redo() {
	if (!CanRedo())
		return false;

	auto command = _redoList.back();
	auto success = command->Execute();
	if (success) {
		_redoList.pop_back();
		_undoList.push_back(command);
	}
	return success;
}

void CommandManager::Clear() {
	_undoList.clear();
	_redoList.clear();
}

AppCommand* CommandManager::GetUndoCommand() const {
	return _undoList.empty() ? nullptr : _undoList.back().get();
}

AppCommand* CommandManager::GetRedoCommand() const {
	return _redoList.empty() ? nullptr : _redoList.back().get();
}
