#pragma once
#include <stdint.h>
#include <string>
#include <utility>

namespace avant::connection
{
    class base_ctx
    {
    public:
        base_ctx() = default;
        virtual ~base_ctx();

        base_ctx(const base_ctx &) = delete;
        base_ctx &operator=(const base_ctx &) = delete;
        base_ctx(base_ctx &&) = delete;
        base_ctx &operator=(base_ctx &&) = delete;

        inline void clear_app_layer_notified()
        {
            this->app_layer_notified = false;
        }

        inline void mark_app_layer_notified()
        {
            this->app_layer_notified = true;
        }

        [[nodiscard]] inline bool get_app_layer_notified() const
        {
            return this->app_layer_notified;
        }

        virtual void on_close() noexcept = 0;
        virtual void on_event(uint32_t event) noexcept = 0;

        virtual std::pair<std::string, int> get_ip_port() const = 0;

    private:
        // Indicates whether the context has been notified to the application layer
        bool app_layer_notified{false};
    };
}
