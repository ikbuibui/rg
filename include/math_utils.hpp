#include <concepts>

namespace rg
{

    /**
     * @brief Computes the modulo of a value with a specified modulus.
     *
     * This function takes an integral value and computes its modulo with a specified modulus.
     * The modulus is specified as a template parameter.
     *
     * @tparam Modulus The modulus to use. Must be greater than 0.
     * @param value The value to compute the modulo of.
     * @return The result of value % Modulus.
     */
    template<std::integral auto Modulus>
    std::integral auto modulo(std::integral auto value) requires(Modulus > 0)
    {
        if constexpr((Modulus & (Modulus - 1)) == 0)
        {
            // Modulus is a power of two
            return value & (Modulus - 1);
        }
        else
        {
            // Modulus is not a power of two
            return value % Modulus;
        }
    }

} // namespace rg
