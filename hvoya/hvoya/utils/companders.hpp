#pragma once

#include <cmath>
#include <type_traits>


namespace hvoya::utils {


    // https://www.desmos.com/calculator/tmftezgivx

    
    template<typename T = double>
    constexpr void alawCompress(T* p, T alpha, size_t len) noexcept {
        // Precompute constants outside the loop
        const T invAlpha = T(1) / alpha;
        const T invOnePlusLnAlpha = T(1) / (T(1) + std::log(alpha));
        const T alphaTimesInvOnePlusLnAlpha = alpha * invOnePlusLnAlpha;
        
        for (size_t i = 0; i < len; ++i) {
            const T absX = std::abs(p[i]);
            const T signX = std::copysign(T(1), p[i]);
            
            if (absX < invAlpha) {
                // Linear region
                p[i] = signX * absX * alphaTimesInvOnePlusLnAlpha;
            } else {
                // Logarithmic region
                p[i] = signX * (T(1) + std::log(alpha * absX)) * invOnePlusLnAlpha;
            }
        }
    }
    

    template<typename T = double>
    constexpr void muLawCompress(T* p, T mu, size_t len) noexcept {
        // Precompute constant outside the loop
        const T invLogOnePlusMu = T(1) / std::log(T(1) + mu);
        
        for (size_t i = 0; i < len; ++i) {
            const T absX = std::abs(p[i]);
            const T signX = std::copysign(T(1), p[i]);
            
            p[i] = signX * std::log(T(1) + mu * absX) * invLogOnePlusMu;
        }
    }
    
    
    
    template <typename T = double>
    class ALawCompander {
        static_assert(std::is_floating_point_v<T>, "Only floating point types supported");
        
    private:
        const T _alpha;
        // precomputed values
        const T _invAlpha;                      // 1/alpha
        const T _invOnePlusLnAlpha;             // 1/(1 + ln(alpha))
        const T _alphaTimesInvOnePlusLnAlpha;   // alpha/(1 + ln(alpha))
        
    public:
        constexpr explicit ALawCompander(T a) noexcept 
            : _alpha(a)
            , _invAlpha(T(1) / a)
            , _invOnePlusLnAlpha(T(1) / (T(1) + std::log(a)))
            , _alphaTimesInvOnePlusLnAlpha(a * _invOnePlusLnAlpha)
        {}
        
        [[nodiscard]] constexpr T compress(T x) const noexcept {
            const T absX = std::abs(x);
            const T signX = std::copysign(T(1), x);
            
            if (absX < _invAlpha) {
                // Linear region: sign(x) * alpha * |x| / (1 + ln(alpha))
                return signX * absX * _alphaTimesInvOnePlusLnAlpha;
            } else {
                // Logarithmic region: sign(x) * (1 + ln(alpha * |x|)) / (1 + ln(alpha))
                return signX * (T(1) + std::log(_alpha * absX)) * _invOnePlusLnAlpha;
            }
        }
    };


    template <typename T = double>
    [[nodiscard]] constexpr T alawCompress(T x, T alpha) noexcept {
        return ALawCompander<T>(alpha).compress(x);
    }

    
    // Optimized compile time alpha
    template <typename T, T alpha>
    [[nodiscard]] constexpr T alawCompressStatic(T x) noexcept {
        constexpr T invAlpha = T(1) / alpha;
        constexpr T invOnePlusLnAlpha = T(1) / (T(1) + std::log(alpha));
        constexpr T alphaTimesInv = alpha * invOnePlusLnAlpha;
        
        const T absX = std::abs(x);
        const T signX = std::copysign(T(1), x);
        
        if (absX < invAlpha) {
            return signX * absX * alphaTimesInv;
        } else {
            return signX * (T(1) + std::log(alpha * absX)) * invOnePlusLnAlpha;
        }
    }
   
    
        

    template <typename T = double>
    class MuLawCompander {
        static_assert(std::is_floating_point_v<T>, "Only floating point types supported");
        
    private:
        const T _mu;
        const T _invLogOnePlusMu;  // 1/log(1 + mu) - precomputed
        
    public:
        constexpr explicit MuLawCompander(T mu) noexcept 
            : _mu(mu)
            , _invLogOnePlusMu(T(1) / std::log(T(1) + mu))
        {}
        
        [[nodiscard]] constexpr T compress(T x) const noexcept {
            const T absX = std::abs(x);
            const T signX = std::copysign(T(1), x);
            
            // sign(x) * log(1 + mu * |x|) / log(1 + mu)
            return signX * std::log(T(1) + _mu * absX) * _invLogOnePlusMu;
        }
    };

    
    template <typename T = double>
    [[nodiscard]] constexpr T muLawCompress(T x, T mu) noexcept {
        return MuLawCompander<T>(mu).compress(x);
    }

    
    // Optimized compile time mu
    template <typename T, T mu>
    [[nodiscard]] constexpr T muLawCompressStatic(T x) noexcept {
        constexpr T invLogOnePlusMu = T(1) / std::log(T(1) + mu);
        
        const T absX = std::abs(x);
        const T signX = std::copysign(T(1), x);
        
        return signX * std::log(T(1) + mu * absX) * invLogOnePlusMu;
    }

    // Example usage:
    /*
    // Runtime mu
    MuLawCompander<double> compressor(255.0);
    double result = compressor.compress(inputSample);

    // Or single call
    double result = muLawCompress(inputSample, 255.0);

    // Compile-time mu (fastest)
    double result = muLawCompressStatic<double, 255.0>(inputSample);
    */

}
