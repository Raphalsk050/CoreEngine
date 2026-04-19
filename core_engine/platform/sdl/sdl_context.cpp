#include "platform/sdl/sdl_context.h"
#include <SDL3/SDL.h>

namespace CoreEngine {
    SdlContext::~SdlContext() {
        Shutdown();
    }

    bool SdlContext::InitializeVideo() {
        if (video_initialized_) {
            return true;
        }
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            return false;
        }
        video_initialized_ = true;
        return true;
    }

    bool SdlContext::InitializeAudio() {
        if (audio_initialized_) {
            return true;
        }

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            return false;
        }

        audio_initialized_ = true;
        return true;
    }

    void SdlContext::Shutdown() {
        if (audio_initialized_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audio_initialized_ = false;
        }

        if (video_initialized_) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            video_initialized_ = false;
        }

        SDL_Quit();
    }
}
