#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <functional>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <type_traits>
#include <cerrno>
#include <cstring>

namespace avant
{
    namespace ipc
    {
        // Fixed-size object pool backed by a POSIX shared memory object
        //
        // Memory layout:
        //   [ header ]
        //   [ padding for atomic_ref<uint8_t> alignment ]
        //   [ use-list: count bytes, 1 byte per slot ]
        //   [ alignment padding for T ]
        //   [ count * T ]
        //
        // The header records magic/version/count/mem_size so a stale object from
        // an earlier (different) configuration is rejected instead of misparsed.
        //
        // Semantics:
        // - init() opens or creates the shm object and maps it.
        //   On creation the header is written and the use-list zeroed.
        //   On re-open the existing state is kept.
        // - The use-list uses std::atomic_ref so the actual shared-memory storage
        //   remains std::uint8_t while accesses to it can be performed atomically.
        // - Synchronization of the T payload itself is NOT provided.
        //   If a slot is shared between processes while in use, the producer /
        //   consumer must order their accesses themselves.
        // - The shm object persists in the kernel until unlink() is called
        //   or the system reboots.
        template <typename T>
        class shm_pool
        {
        public:
            shm_pool(const std::string &name, size_t count);
            ~shm_pool();

            // Open or create the shm object and map it.
            bool init();

            // Unmap and close the fd.
            // The shm object itself stays in the kernel.
            bool close();

            // close() + remove the object from the kernel.
            bool unlink();

            // Mark all slots free.
            bool reset();

            // Claim a slot.
            // Returns nullptr when the pool is exhausted.
            T *alloc();

            // Release a slot previously obtained from alloc().
            bool back(T *t);

            // Iterate slots; callback return false stops the iteration.
            // used=true visits in-use slots only (default),
            // used=false visits free slots only.
            void foreach (std::function<bool(T *, bool)> callback, bool used = true);

        private:
            struct header
            {
                std::uint64_t magic{0};
                std::uint32_t version{0};
                std::uint32_t count{0};
                std::uint64_t mem_size{0};
            };

            static constexpr std::uint64_t M_MAGIC = 0x314350495641ULL; // "AVIPC1"
            static constexpr std::uint32_t M_VERSION = 1;

            T *get_by_index(size_t index);
            header *hdr();
            T *object_base();

            // The actual shared-memory storage is std::uint8_t.
            // std::atomic_ref is used when atomic access is required.
            std::uint8_t *use_list_base();

        private:
            size_t count{0};

            // Padding between use-list and T object array.
            size_t padding{0};

            // Padding between header and use-list.
            size_t use_list_padding{0};

            size_t mem_size{0};

            std::string name{};
            int mem_fd{-1};
            void *mem_ptr{nullptr};
        };

        template <typename T>
        shm_pool<T>::shm_pool(
            const std::string &name,
            size_t count)
            : count(count),
              name(name)
        {
            /*
             * The use-list is accessed through std::atomic_ref<uint8_t>.
             * Therefore its address must satisfy atomic_ref's alignment
             * requirement.
             */
            constexpr size_t use_list_alignment =
                std::atomic_ref<std::uint8_t>::required_alignment;

            constexpr size_t object_alignment =
                std::alignment_of<T>::value;

            /*
             * Layout:
             *
             * [ header ]
             * [ use-list padding ]
             * [ use-list ]
             * [ T padding ]
             * [ T objects ]
             */

            const size_t pre_use_list_size = sizeof(header);

            // use_list_padding is 0
            this->use_list_padding =
                (use_list_alignment -
                 pre_use_list_size % use_list_alignment) %
                use_list_alignment;

            const size_t use_list_offset =
                sizeof(header) + this->use_list_padding;

            const size_t pre_obj_size =
                use_list_offset +
                sizeof(std::uint8_t) * count;

            this->padding =
                (object_alignment -
                 pre_obj_size % object_alignment) %
                object_alignment;

            this->mem_size =
                pre_obj_size +
                this->padding +
                sizeof(T) * count;
        }

        template <typename T>
        shm_pool<T>::~shm_pool()
        {
            close();
        }

        template <typename T>
        bool shm_pool<T>::init()
        {
            if (this->mem_ptr != nullptr)
            {
                return true; // already mapped
            }

            int shm_fd = shm_open(
                this->name.c_str(),
                O_RDWR,
                0);

            bool recreate = false;

            if (shm_fd == -1)
            {
                // Create.
                if (errno != ENOENT)
                {
                    return false;
                }

                shm_fd = shm_open(
                    this->name.c_str(),
                    O_RDWR | O_CREAT | O_EXCL,
                    S_IRUSR | S_IWUSR);

                if (shm_fd == -1)
                {
                    return false;
                }

                // shm space size.
                // The kernel may round the object size up to a page.
                if (-1 == ftruncate(
                              shm_fd,
                              static_cast<off_t>(this->mem_size)))
                {
                    ::close(shm_fd);
                    shm_unlink(this->name.c_str());
                    return false;
                }

                recreate = true;
            }
            else
            {
                // The existing object must be at least our required size.
                struct stat st;

                if (-1 == fstat(shm_fd, &st) ||
                    static_cast<size_t>(st.st_size) < this->mem_size)
                {
                    ::close(shm_fd);
                    return false;
                }
            }

            // Mapping.
            void *mem_ptr = mmap(
                nullptr,
                this->mem_size,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                shm_fd,
                0);

            if (mem_ptr == MAP_FAILED)
            {
                ::close(shm_fd);

                if (recreate)
                {
                    shm_unlink(this->name.c_str());
                }

                return false;
            }

            if (recreate)
            {
                // Newly created shared memory:
                // initialize metadata and use-list.
                header *h =
                    static_cast<header *>(mem_ptr);

                h->magic = M_MAGIC;
                h->version = M_VERSION;
                h->count =
                    static_cast<std::uint32_t>(this->count);
                h->mem_size = this->mem_size;

                std::uint8_t *use_list =
                    static_cast<std::uint8_t *>(mem_ptr) +
                    sizeof(header) +
                    this->use_list_padding;

                std::memset(
                    use_list,
                    0,
                    this->count * sizeof(std::uint8_t));
            }
            else
            {
                // Re-use existing shared memory:
                // verify metadata before using it.
                header *h =
                    static_cast<header *>(mem_ptr);

                if (h->magic != M_MAGIC ||
                    h->version != M_VERSION ||
                    h->count !=
                        static_cast<std::uint32_t>(this->count) ||
                    h->mem_size != this->mem_size)
                {
                    ::munmap(mem_ptr, this->mem_size);
                    ::close(shm_fd);
                    return false;
                }
            }

            this->mem_ptr = mem_ptr;
            this->mem_fd = shm_fd;

            return true;
        }

        template <typename T>
        bool shm_pool<T>::close()
        {
            bool ok = true;

            if (this->mem_ptr != nullptr)
            {
                if (-1 == munmap(
                              this->mem_ptr,
                              this->mem_size))
                {
                    ok = false;
                }

                this->mem_ptr = nullptr;
            }

            if (this->mem_fd != -1)
            {
                if (-1 == ::close(this->mem_fd))
                {
                    ok = false;
                }

                this->mem_fd = -1;
            }

            return ok;
        }

        template <typename T>
        bool shm_pool<T>::unlink()
        {
            close();

            return (shm_unlink(
                        this->name.c_str()) != -1);
        }

        template <typename T>
        bool shm_pool<T>::reset()
        {
            if (this->mem_ptr == nullptr)
            {
                return false;
            }

            std::uint8_t *use_list =
                use_list_base();

            for (size_t i = 0; i < this->count; ++i)
            {
                std::atomic_ref<std::uint8_t> slot(use_list[i]);

                slot.store(
                    0,
                    std::memory_order_relaxed);
            }

            return true;
        }

        template <typename T>
        T *shm_pool<T>::alloc()
        {
            if (this->mem_ptr == nullptr)
            {
                return nullptr;
            }

            std::uint8_t *use_list =
                use_list_base();

            T *object_ptr =
                object_base();

            for (size_t i = 0; i < this->count; ++i)
            {
                std::atomic_ref<std::uint8_t> slot(
                    use_list[i]);

                /*
                 * Atomically claim the slot.
                 *
                 * exchange() returns the previous value:
                 *
                 *   0 -> successfully claimed
                 *   1 -> already in use
                 */
                if (slot.exchange(
                        1,
                        std::memory_order_acq_rel) == 0)
                {
                    return &object_ptr[i];
                }
            }

            return nullptr;
        }

        template <typename T>
        bool shm_pool<T>::back(T *t)
        {
            if (this->mem_ptr == nullptr)
            {
                return false;
            }

            T *start_addr =
                object_base();

            T *end_addr =
                start_addr + this->count;

            if (t < start_addr || t >= end_addr)
            {
                return false;
            }

            size_t gap =
                reinterpret_cast<char *>(t) -
                reinterpret_cast<char *>(start_addr);

            if (gap % sizeof(T) != 0)
            {
                return false;
            }

            size_t index =
                gap / sizeof(T);

            std::uint8_t *use_list =
                use_list_base();

            std::atomic_ref<std::uint8_t> slot(
                use_list[index]);

            /*
             * Atomically release the slot.
             *
             * exchange() returns the previous value:
             *
             *   1 -> successfully released
             *   0 -> not in use / double back
             */
            if (slot.exchange(
                    0,
                    std::memory_order_acq_rel) != 1)
            {
                return false;
            }

            return true;
        }

        template <typename T>
        T *shm_pool<T>::get_by_index(size_t index)
        {
            if (index >= this->count)
            {
                return nullptr;
            }

            return object_base() + index;
        }

        template <typename T>
        void shm_pool<T>::foreach(
            std::function<bool(T *, bool)> callback,
            bool used)
        {
            if (this->mem_ptr == nullptr)
            {
                return;
            }

            std::uint8_t *use_list =
                use_list_base();

            for (size_t i = 0; i < this->count; ++i)
            {
                std::atomic_ref<std::uint8_t> slot(
                    use_list[i]);

                bool slot_used =
                    slot.load(
                        std::memory_order_relaxed) != 0;

                if (slot_used == used)
                {
                    if (!callback(
                            get_by_index(i),
                            slot_used))
                    {
                        break;
                    }
                }
            }
        }

        template <typename T>
        typename shm_pool<T>::header *
        shm_pool<T>::hdr()
        {
            return static_cast<header *>(
                this->mem_ptr);
        }

        template <typename T>
        T *shm_pool<T>::object_base()
        {
            return reinterpret_cast<T *>(
                static_cast<char *>(this->mem_ptr) +
                sizeof(header) +
                this->use_list_padding +
                sizeof(std::uint8_t) * this->count +
                this->padding);
        }

        template <typename T>
        std::uint8_t *
        shm_pool<T>::use_list_base()
        {
            return reinterpret_cast<std::uint8_t *>(
                static_cast<char *>(this->mem_ptr) +
                sizeof(header) +
                this->use_list_padding);
        }

    } // namespace ipc
} // namespace avant
