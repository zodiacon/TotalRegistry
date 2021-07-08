#pragma once

struct AppCommand;

class CommandManager {
public:
	bool CanUndo() const;
	bool CanRedo() const;

	bool AddCommand(std::shared_ptr<AppCommand> command, bool execute = true);
	bool Undo();
	bool Redo();
	void Clear();

	AppCommand* GetUndoCommand() const;
	AppCommand* GetRedoCommand() const;

private:
	std::vector<std::shared_ptr<AppCommand>> _undoList;
	std::vector<std::shared_ptr<AppCommand>> _redoList;
};

