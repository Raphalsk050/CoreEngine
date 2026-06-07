#include "controller.h"

#include <string>
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
            UpdateCamera();
            UpdatePossessed(frame.delta_time);
        }
    }

    bool Controller::BindControls() const {
        bool success = true;
        success &= context_.input_system.BindAxis2D(MoveAction, CoreEngine::Key::A, CoreEngine::Key::D,
                                                    CoreEngine::Key::S, CoreEngine::Key::W);

        return success;
    }

    void Controller::UpdateCamera() const {
        const auto camera_info = camera_->GetCameraInfo();

        const float camera_yaw =
                CoreEngine::Math::Deg2Rad(mouse_delta_.x * camera_info.camera_mouse_look_speed_degrees);
        const float camera_pitch =
                CoreEngine::Math::Deg2Rad(-mouse_delta_.y * camera_info.camera_mouse_look_speed_degrees);

        camera_->SetCameraPosition(possessed_->GetNode().GetPosition());
        camera_->AddLookDelta(camera_yaw, camera_pitch);
    }

    void Controller::UpdatePossessed(float delta_time) {
        if (possessed_ != nullptr) {
            possessed_->AddMovementInput(context_.input_system.GetAxis2D(MoveAction), delta_time);
        }
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
