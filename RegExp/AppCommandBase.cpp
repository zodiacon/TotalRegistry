#include "pch.h"
#include "AppCommandBase.h"

void AppCommandList::AddCommand(std::shared_ptr<AppCommand> command) {
	m_Commands.push_back(command);
}

std::shared_ptr<AppCommand> AppCommandList::GetCommand(size_t i) const {
	return i < m_Commands.size() ? m_Commands[i] : nullptr;
}

int AppCommandList::GetCount() const {
	return static_cast<int>(m_Commands.size());
}

bool AppCommandList::Execute() {
	for (auto& cmd : m_Commands)
		if (!cmd->Execute())
			return false;
	return InvokeCallback(true);
}

bool AppCommandList::Undo() {
	for (int i = (int)m_Commands.size() - 1; i >= 0; --i)
		if (!m_Commands[i]->Undo())
			return false;

	return InvokeCallback(false);
}
