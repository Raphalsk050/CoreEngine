#pragma once


namespace CoreEngine {
    class SdlContext {
    public:
        ~SdlContext();

        [[nodiscard]] bool InitializeVideo();

        [[nodiscard]] bool InitializeAudio();


        void Shutdown();

    private:
        bool video_initialized_ = false;
        bool audio_initialized_ = false;
    };
} // namespace CoreEngine
