#pragma once
#include <memory>

namespace avant
{
    namespace utility
    {
        /**
         * @brief singleton template
         *
         * @tparam T
         */
        template <typename T>
        class singleton
        {
        public:
            template <typename... Args>
            [[nodiscard]] static T *instance(Args &&...args)
            {
                if (m_instance == nullptr)
                {
                    m_instance = std::make_shared<T>(std::forward<Args>(args)...);
                }
                return m_instance.get();
            }
            [[nodiscard]] static T *get_instance()
            {
                return m_instance.get();
            }
            static void destory_instance()
            {
                m_instance.reset();
            }

        protected:
            singleton() = default;
            ~singleton() = default;
            static std::shared_ptr<T> m_instance;
        };

        template <typename T>
        std::shared_ptr<T> singleton<T>::m_instance = nullptr;
    }
}
