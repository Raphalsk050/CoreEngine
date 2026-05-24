#include "core/editor/editor_command_history.h"

#include <algorithm>

namespace CoreEngine {
    EditorCommandHistory::EditorCommandHistory(std::size_t max_commands)
        : max_commands_(std::max<std::size_t>(max_commands, 1u)) {
        undo_stack_.reserve(max_commands_);
        redo_stack_.reserve(max_commands_);
    }

    bool EditorCommandHistory::Execute(EditorCommandContext &context, std::unique_ptr<IEditorCommand> command) {
        if (command == nullptr || !command->Execute(context)) {
            return false;
        }

        redo_stack_.clear();
        undo_stack_.push_back(std::move(command));
        if (undo_stack_.size() > max_commands_) {
            undo_stack_.erase(undo_stack_.begin());
        }
        return true;
    }

    bool EditorCommandHistory::Undo(EditorCommandContext &context) {
        if (undo_stack_.empty()) {
            return false;
        }

        std::unique_ptr<IEditorCommand> command = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        if (!command->Undo(context)) {
            undo_stack_.push_back(std::move(command));
            return false;
        }

        redo_stack_.push_back(std::move(command));
        return true;
    }

    bool EditorCommandHistory::Redo(EditorCommandContext &context) {
        if (redo_stack_.empty()) {
            return false;
        }

        std::unique_ptr<IEditorCommand> command = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        if (!command->Execute(context)) {
            redo_stack_.push_back(std::move(command));
            return false;
        }

        undo_stack_.push_back(std::move(command));
        if (undo_stack_.size() > max_commands_) {
            undo_stack_.erase(undo_stack_.begin());
        }
        return true;
    }

    void EditorCommandHistory::Clear() {
        undo_stack_.clear();
        redo_stack_.clear();
    }

    bool EditorCommandHistory::CanUndo() const noexcept {
        return !undo_stack_.empty();
    }

    bool EditorCommandHistory::CanRedo() const noexcept {
        return !redo_stack_.empty();
    }

    std::string_view EditorCommandHistory::UndoName() const noexcept {
        return undo_stack_.empty() ? std::string_view{} : undo_stack_.back()->Name();
    }

    std::string_view EditorCommandHistory::RedoName() const noexcept {
        return redo_stack_.empty() ? std::string_view{} : redo_stack_.back()->Name();
    }
} // namespace CoreEngine
