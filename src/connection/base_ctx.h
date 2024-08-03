#pragma once
#include <stdint.h>

namespace avant::connection
{
    class base_ctx
    {
    public:
        base_ctx();
        virtual ~base_ctx();

        inline void clear_app_layer_notified()
        {
            this->app_layer_notified = false;
        }

        inline void set_app_layer_notified()
        {
            this->app_layer_notified = true;
        }

        inline bool get_app_layer_notified()
        {

            return this->app_layer_notified;
        }

        virtual void on_close() = 0;
        virtual void on_event(uint32_t event) = 0;

    private:
        // Indicates whether the context has been notified to the application layer
        bool app_layer_notified{false};
    };
}