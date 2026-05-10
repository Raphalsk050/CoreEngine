#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/assert/assert.h"

namespace CoreEngine {
    enum class FutureState {
        Invalid,
        Pending,
        Ready,
        Failed,
        Cancelled,
    };

    // Move-only callable wrapper used by futures so callbacks can capture unique
    // ownership without depending on std::function.
    template<typename Signature>
    class FutureCallback;

    template<typename ReturnType, typename... Args>
    class FutureCallback<ReturnType(Args...)> {
    public:
        FutureCallback() = default;

        template<typename Callable,
                 typename = std::enable_if_t<!std::is_same_v<std::decay_t<Callable>, FutureCallback>>>
        explicit FutureCallback(Callable &&callable)
            : callable_(MakeCallableModel(std::forward<Callable>(callable))) {
        }

        FutureCallback(FutureCallback &&) noexcept = default;
        FutureCallback &operator=(FutureCallback &&) noexcept = default;

        FutureCallback(const FutureCallback &) = delete;
        FutureCallback &operator=(const FutureCallback &) = delete;

        ReturnType operator()(Args... args) {
            CENGINE_ASSERT(callable_ != nullptr, "Cannot invoke an empty future callback");

            if constexpr (std::is_void_v<ReturnType>) {
                callable_->Invoke(std::forward<Args>(args)...);
            } else {
                return callable_->Invoke(std::forward<Args>(args)...);
            }
        }

        [[nodiscard]] explicit operator bool() const {
            return callable_ != nullptr;
        }

    private:
        struct CallableConcept {
            virtual ~CallableConcept() = default;
            virtual ReturnType Invoke(Args... args) = 0;
        };

        template<typename Callable>
        struct CallableModel final : CallableConcept {
            explicit CallableModel(Callable &&callable)
                : callable_(std::move(callable)) {
            }

            ReturnType Invoke(Args... args) override {
                if constexpr (std::is_void_v<ReturnType>) {
                    std::invoke(callable_, std::forward<Args>(args)...);
                } else {
                    return std::invoke(callable_, std::forward<Args>(args)...);
                }
            }

            Callable callable_;
        };

        template<typename Callable>
        [[nodiscard]] static std::unique_ptr<CallableConcept> MakeCallableModel(Callable &&callable) {
            using StoredCallable = std::decay_t<Callable>;
            return std::make_unique<CallableModel<StoredCallable>>(StoredCallable{std::forward<Callable>(callable)});
        }

        std::unique_ptr<CallableConcept> callable_;
    };

    template<typename T>
    class FutureResult {
    public:
        static_assert(!std::is_void_v<T>, "FutureResult<void> is not implemented yet");

        [[nodiscard]] static FutureResult Success(T value) {
            FutureResult result;
            result.state_ = FutureState::Ready;
            result.value_ = std::move(value);
            return result;
        }

        [[nodiscard]] static FutureResult Failure(std::string error_message) {
            FutureResult result;
            result.state_ = FutureState::Failed;
            result.error_message_ = std::move(error_message);
            return result;
        }

        [[nodiscard]] static FutureResult Cancelled(std::string error_message = {}) {
            FutureResult result;
            result.state_ = FutureState::Cancelled;
            result.error_message_ = std::move(error_message);
            return result;
        }

        [[nodiscard]] FutureState State() const {
            return state_;
        }

        [[nodiscard]] bool IsReady() const {
            return state_ == FutureState::Ready ||
                   state_ == FutureState::Failed ||
                   state_ == FutureState::Cancelled;
        }

        [[nodiscard]] bool IsSuccess() const {
            return state_ == FutureState::Ready && value_.has_value();
        }

        [[nodiscard]] bool IsFailed() const {
            return state_ == FutureState::Failed;
        }

        [[nodiscard]] bool IsCancelled() const {
            return state_ == FutureState::Cancelled;
        }

        [[nodiscard]] const T &Value() const {
            CENGINE_ASSERT(IsSuccess(), "Cannot read the value from an unsuccessful future result");
            return *value_;
        }

        [[nodiscard]] T MoveValue() {
            CENGINE_ASSERT(IsSuccess(), "Cannot move the value from an unsuccessful future result");
            return std::move(*value_);
        }

        [[nodiscard]] const std::string &ErrorMessage() const {
            return error_message_;
        }

    private:
        FutureState state_ = FutureState::Invalid;
        std::optional<T> value_;
        std::string error_message_;
    };

    template<typename T>
    class Future;

    template<typename T>
    struct FutureSharedState final {
        using Callback = FutureCallback<void(const FutureResult<T> &)>;

        mutable std::mutex mutex;
        std::condition_variable ready_event;
        std::optional<FutureResult<T>> result;
        std::vector<Callback> callbacks;
    };

    // Producer-side endpoint for completing a Future.
    template<typename T>
    class FuturePromise {
    public:
        FuturePromise()
            : state_(std::make_shared<FutureSharedState<T>>()) {
        }

        [[nodiscard]] Future<T> GetFuture() const {
            return Future<T>{state_};
        }

        bool Resolve(T value) const {
            return Complete(FutureResult<T>::Success(std::move(value)));
        }

        bool Reject(std::string error_message) const {
            return Complete(FutureResult<T>::Failure(std::move(error_message)));
        }

        bool Cancel(std::string error_message = {}) const {
            return Complete(FutureResult<T>::Cancelled(std::move(error_message)));
        }

    private:
        [[nodiscard]] bool Complete(FutureResult<T> result) const {
            if (state_ == nullptr) {
                return false;
            }

            std::vector<typename FutureSharedState<T>::Callback> callbacks;
            {
                std::lock_guard lock{state_->mutex};
                if (state_->result.has_value()) {
                    return false;
                }

                state_->result = std::move(result);
                callbacks = std::move(state_->callbacks);
                state_->callbacks.clear();
            }

            state_->ready_event.notify_all();

            const FutureResult<T> &completed_result = *state_->result;
            for (auto &callback: callbacks) {
                if (callback) {
                    callback(completed_result);
                }
            }

            return true;
        }

        std::shared_ptr<FutureSharedState<T>> state_;
    };

    // Shallow-copy asynchronous result handle.
    //
    // Continuations registered with Then/OnComplete are executed on the thread
    // that completes the associated promise. Asset loaders complete render
    // resources from the render thread after foreground GPU upload.
    template<typename T>
    class Future {
    public:
        using Result = FutureResult<T>;
        using Callback = FutureCallback<void(const Result &)>;

        Future() = default;

        [[nodiscard]] static Future Ready(T value) {
            FuturePromise<T> promise;
            Future future = promise.GetFuture();
            promise.Resolve(std::move(value));
            return future;
        }

        [[nodiscard]] static Future Failed(std::string error_message) {
            FuturePromise<T> promise;
            Future future = promise.GetFuture();
            promise.Reject(std::move(error_message));
            return future;
        }

        [[nodiscard]] static Future Cancelled(std::string error_message = {}) {
            FuturePromise<T> promise;
            Future future = promise.GetFuture();
            promise.Cancel(std::move(error_message));
            return future;
        }

        [[nodiscard]] bool IsValid() const {
            return state_ != nullptr;
        }

        [[nodiscard]] FutureState State() const {
            if (state_ == nullptr) {
                return FutureState::Invalid;
            }

            std::lock_guard lock{state_->mutex};
            if (!state_->result.has_value()) {
                return FutureState::Pending;
            }

            return state_->result->State();
        }

        [[nodiscard]] bool IsReady() const {
            const FutureState state = State();
            return state == FutureState::Ready ||
                   state == FutureState::Failed ||
                   state == FutureState::Cancelled;
        }

        void Wait() const {
            if (state_ == nullptr) {
                return;
            }

            std::unique_lock lock{state_->mutex};
            state_->ready_event.wait(lock, [this] {
                return state_->result.has_value();
            });
        }

        template<typename Callable>
        void Then(Callable &&callable) const {
            OnComplete(std::forward<Callable>(callable));
        }

        template<typename Callable>
        void OnComplete(Callable &&callable) const {
            if (state_ == nullptr) {
                return;
            }

            Callback callback{std::forward<Callable>(callable)};
            const Result *completed_result = nullptr;
            {
                std::lock_guard lock{state_->mutex};
                if (state_->result.has_value()) {
                    completed_result = &*state_->result;
                } else {
                    state_->callbacks.push_back(std::move(callback));
                    return;
                }
            }

            if (callback) {
                callback(*completed_result);
            }
        }

    private:
        friend class FuturePromise<T>;

        explicit Future(std::shared_ptr<FutureSharedState<T>> state)
            : state_(std::move(state)) {
        }

        std::shared_ptr<FutureSharedState<T>> state_;
    };
} // namespace CoreEngine
