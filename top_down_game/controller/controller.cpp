#include "controller.h"

#include "core/application/engine_context.h"
#include "core/application/frame_context.h"
#include "core/log/log.h"
#include "core/log/logger.h"

namespace TopDownGame {
    Controller::Controller(const CoreEngine::EngineContext &context) : context_(context) {

        // TODO(rafael): think this in a more dynamic way in the future to bind all the necessary inputs from a json
        if (BindControls()) {
            CoreEngine::Log::Info("Controller", "Controller binds were completed");
        } else {
            CoreEngine::Log::Error("Controller", "Controller binds were failed");
        }

        camera_ = std::make_unique<Camera>(context);
    }

    void Controller::Update(const CoreEngine::FrameContext &frame) {
        if (camera_ != nullptr) {
            mouse_delta_ = frame.input_system.MouseDelta();
            UpdateCamera(frame);
        }
    }

    bool Controller::BindControls() const {
        bool success = true;
        success &= context_.input_system.BindAxis2D(MoveAction, CoreEngine::Key::A, CoreEngine::Key::D,
                                                    CoreEngine::Key::S, CoreEngine::Key::W);

        return success;
    }

    void Controller::UpdateCamera(const CoreEngine::FrameContext &frame) const {
        const auto camera_info = camera_->GetCameraInfo();
        auto camera_yaw = mouse_delta_.x * camera_info.camera_mouse_look_speed * frame.delta_time;
        auto camera_pitch = mouse_delta_.y * camera_info.camera_mouse_look_speed * frame.delta_time;

        camera_yaw = CoreEngine::Math::Deg2Rad(camera_yaw);
        camera_pitch = CoreEngine::Math::Deg2Rad(camera_pitch);

        camera_->AddLookDelta(camera_yaw, camera_pitch);
    }
    void Controller::Possess(IPossessable &possessable) {

        if (possessed_ == &possessable) {
            return;
        }

        Unpossess();

        possessed_ = &possessable;
        possessable.OnPossessed();
    }

    void Controller::Unpossess() {
        if (possessed_ == nullptr) {
            return;
        }

        possessed_->OnUnpossessed();
        possessed_ = nullptr;
    }
} // namespace TopDownGame
