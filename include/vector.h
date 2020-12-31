#include <stdint.h>
#include <math.h>

namespace csso
{
    template <typename T, const uint16_t max_size>
    struct array final
    {
    public:
        array() = default;

        array(const array &other) = delete;
        array &operator=(const array &other) = delete;

        void push_back(const T &v)
        {
            if (allocatedSize >= max_size)
            {
                return;
            }

            this->v[allocatedSize] = v;
            ++allocatedSize;
        }

        void pop_back()
        {
            if (allocatedSize > 0)
            {
                --allocatedSize;
            }
        }

        T &operator[](const uint16_t index)
        {
            if (index < allocatedSize)
            {
                return v[index];
            }
            else
            {
                static T def;
                def = T{};
                return def;
            }
        }

        T &last()
        {
            return this->operator[](allocatedSize - 1);
        }

        uint16_t count()
        {
            return allocatedSize;
        }

    private:
        T v[max_size];
        uint16_t allocatedSize = 0;
    };
} // namespace csso
