#include <stdint.h>
#include <math.h>

namespace csso
{
    template <typename T, const uint16_t max_size>
    struct cycle_array final
    {
    public:
        explicit cycle_array(const T &def)
        {
            for (uint16_t i = 0; i < max_size; i++)
            {
                v[i] = def;
            }
        }

        cycle_array() = default;
        cycle_array(const cycle_array &other) = delete;
        cycle_array &operator=(const cycle_array &other) = delete;

        void push(const T &value)
        {
            v[current] = value;
            current = (current + 1) % max_size;
        }

        constexpr uint16_t count() const
        {
            return max_size;
        }

        T &operator[](const uint16_t index)
        {
            return v[index];
        }

        const T &operator[](const uint16_t index) const
        {
            return v[index];
        }

    private:
        T v[max_size];
        uint16_t current = 0;
    };
} // namespace csso
