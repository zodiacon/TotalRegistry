#include "pch.h"
#include "CommandManager.h"
#include "AppCommandBase.h"

void CommandManager::Enable(bool enable) {
	m_Enabled = enable;
}

bool CommandManager::IsEnabled() const {
	return m_Enabled;
}

bool CommandManager::CanUndo() const {
	return !m_UndoList.empty();
}

bool CommandManager::CanRedo() const {
	return !m_RedoList.empty();
}

bool CommandManager::AddCommand(std::shared_ptr<AppCommand> command, bool execute) {
	if (execute)
		if (!command->Execute())
			return false;

	if (!m_Enabled)
		return true;

	m_UndoList.push_back(command);
	m_RedoList.clear();
	return true;
}

bool CommandManager::Undo() {
	if (!CanUndo())
		return false;

	auto cmd = m_UndoList.back();
	auto success = cmd->Undo();
	if (success) {
		m_RedoList.push_back(cmd);
		m_UndoList.pop_back();
	}
	return success;
}

bool CommandManager::Redo() {
	if (!CanRedo())
		return false;

	auto command = m_RedoList.back();
	auto success = command->Execute();
	if (success) {
		m_RedoList.pop_back();
		m_UndoList.push_back(command);
	}
	return success;
}

void CommandManager::Clear() {
	m_UndoList.clear();
	m_RedoList.clear();
}

AppCommand* CommandManager::GetUndoCommand() const {
	return m_UndoList.empty() ? nullptr : m_UndoList.back().get();
}

AppCommand* CommandManager::GetRedoCommand() const {
	return m_RedoList.empty() ? nullptr : m_RedoList.back().get();
}
