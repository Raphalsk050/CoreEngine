#pragma once


namespace CoreEngine {
    class SdlContext {
    public:
        ~SdlContext();

        [[nodiscard]] bool InitializeVideo();


        void Shutdown();

    private:
        bool video_initialized_ = false;
    };
}
