#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace CoreEngine {
    class RenderSystem;
    class World;

    struct EditorCommandContext {
        World &world;
        RenderSystem &render_system;
    };

    /**
     * @brief Describes a reversible editor action.
     *
     * Responsibility: encapsulate slow authoring mutations outside runtime hot
     * paths so editor undo/redo stays explicit and isolated from gameplay code.
     */
    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;

        [[nodiscard]] virtual std::string_view Name() const noexcept = 0;

        [[nodiscard]] virtual bool Execute(EditorCommandContext &context) = 0;

        [[nodiscard]] virtual bool Undo(EditorCommandContext &context) = 0;
    };

    /**
     * @brief Owns bounded editor undo and redo stacks.
     *
     * Responsibility: keep command lifetime deterministic and cap memory usage
     * for authoring-only history without coupling commands to UI widgets.
     */
    class EditorCommandHistory final {
    public:
        explicit EditorCommandHistory(std::size_t max_commands = 128);

        [[nodiscard]] bool Execute(EditorCommandContext &context, std::unique_ptr<IEditorCommand> command);

        [[nodiscard]] bool Undo(EditorCommandContext &context);

        [[nodiscard]] bool Redo(EditorCommandContext &context);

        void Clear();

        [[nodiscard]] bool CanUndo() const noexcept;

        [[nodiscard]] bool CanRedo() const noexcept;

        [[nodiscard]] std::string_view UndoName() const noexcept;

        [[nodiscard]] std::string_view RedoName() const noexcept;

    private:
        std::size_t max_commands_ = 128;
        std::vector<std::unique_ptr<IEditorCommand>> undo_stack_;
        std::vector<std::unique_ptr<IEditorCommand>> redo_stack_;
    };
} // namespace CoreEngine
