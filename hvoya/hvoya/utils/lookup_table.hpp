#include <cassert>
#include <cmath>
#include <array>
#include <cstdint>
#include <functional>    

#include "types.hpp"


namespace hvoya {


    using lut_gen_func_t = std::function <void (uint_fast16_t, sample_t*)>;
    
    /*
    example:
    
        auto yourGenerator = [] (uint_fast16_t numVals, sample_t* pVals) {
            assert (numVals == <yourNumVals>);
            for (size_t i = 0; i < numVals; ++i)
                pVals [i] = <your formula>;
        };
        
        LUT <yourNumVals> _lut;
        _lut.generate (yourGenerator);
    */
   
    constexpr auto sigmoidLUTGen = [] (uint_fast16_t numVals, sample_t* pVals) {
        for (size_t i = 0; i < numVals / 2; ++i) {
            sample_t norm = sample_t (i) / (numVals / 2);
            auto v = norm * norm / 2;
            assert (v >= 0. && v <= 1.);
            pVals [i] = v;
            pVals [numVals - i - 1] = 1 - v;
        }
    };


    class ILUT {
        public:
            virtual constexpr void generate (lut_gen_func_t) = 0;
            virtual constexpr uint_fast16_t size() const = 0;
            
            virtual constexpr sample_t getByNormalizedPhase (sample_t p) const = 0;
            
            // use this if the generator produced only the positive half
            // of some symmetrical function like sine or triangle
            virtual constexpr sample_t getByNormalizedPhaseMirrored (sample_t p) const = 0;

            virtual constexpr const sample_t& operator[] (uint_fast16_t i) const = 0;
            virtual constexpr       sample_t& operator[] (uint_fast16_t i)       = 0;

            virtual void operator= (const ILUT&) {};
            
            virtual constexpr sample_t* data()  noexcept = 0;
            virtual constexpr sample_t* begin() noexcept = 0;
            virtual constexpr sample_t* end()   noexcept = 0;
            
            virtual constexpr const sample_t* cdata()  const noexcept = 0;
            virtual constexpr const sample_t* cbegin() const noexcept = 0;
            virtual constexpr const sample_t* cend()   const noexcept = 0;
    };


    
    
    template <uint_fast16_t N>
    class LUT : public ILUT {
        static_assert ((N > 0 && ((N & (N - 1)) == 0)), "use power of 2 size");

        std::array <sample_t, N> values;

        // Value-domain (non-normalized) state for getClamped*: the table spans the symmetric
        // domain [-_halfRange, +_halfRange], with index N/2 mapped to *exactly* v = 0. Default
        // leaves a [-1, 1] domain; generateClamped() bakes the real domain + curve.
        sample_t _halfRange = 1;
        sample_t _scale     = sample_t (N) / sample_t (2);   // index units per input unit = N / (2*_halfRange)

        public:

            constexpr LUT () : values() {}

            constexpr LUT (lut_gen_func_t gen)
                : values() {
                gen (N, values.data());
                check();
            }

            void constexpr generate (lut_gen_func_t gen) override {
                gen (N, values.data());
                check();
            }

            // Bake a value-domain table over the symmetric domain [-halfRange, +halfRange] for
            // use with getClampedLinear / getClampedQuadratic. The generator is handed the *input
            // value* at each sample (not an index). Index N/2 lands on exactly v = 0, so an odd
            // function (tanh, asinh, …) is bit-exact 0 there — getClamped* then return exactly 0
            // at v = 0 with no special-case branch.
            void generateClamped (sample_t halfRange, const std::function <sample_t (sample_t)>& fnOfValue) {
                assert (halfRange > 0);
                _halfRange = halfRange;
                _scale     = sample_t (N) / (sample_t (2) * halfRange);
                const sample_t step = (sample_t (2) * halfRange) / sample_t (N);   // exact: N is pow2
                constexpr int center = N / 2;
                for (uint_fast16_t i = 0; i < N; ++i)
                    values [i] = fnOfValue ((int (i) - center) * step);
                check();
            }

            inline constexpr void check () const {
                for (const auto& v : values)
					assert (std::fpclassify (v) != FP_SUBNORMAL && "subnormal LUT value");
            }

            constexpr uint_fast16_t size() const override { return N; }

            constexpr sample_t* data()  noexcept override { return values.data(); }
            constexpr sample_t* begin() noexcept override { return values.data(); }
            constexpr sample_t* end()   noexcept override { return values.data() + N; }

            constexpr const sample_t* cdata()  const noexcept override { return values.data(); }
            constexpr const sample_t* cbegin() const noexcept override { return values.data(); }
            constexpr const sample_t* cend()   const noexcept override { return values.data() + N; }

            constexpr const sample_t& operator[] (uint_fast16_t i) const override {
                assert (i < N);
                return values [i];
            }

            constexpr sample_t& operator[] (uint_fast16_t i) override {
                assert (i < N);
                return values [i];
            }

            void operator= (const ILUT& other) override {
                assert (other.size() == size());
                std::copy (other.cdata(), other.cdata() + other.size(),
                        data());
                check();
            }

            inline constexpr sample_t getByNormalizedPhase (sample_t p) const override {
                assert (p >= 0 && p <= 1);
                p *= N;
                uint_fast16_t ip = p;
                uint_fast16_t i1 = ip & (N - 1);
                uint_fast16_t i2 = (i1 + 1) & (N - 1);
                sample_t f = p - ip;
                return (1 - f) * values [i1] + f * values [i2];
            }
            
            inline constexpr sample_t getByNormalizedPhaseMirrored (sample_t p) const override {
                assert (p >= 0 && p < 1);
                sample_t sign = 1;
                if (p >= 0.5) {
                    p -= 0.5;
                    sign = -1;
                }
                p *= 2;
                return sign * getByNormalizedPhase (p);
            }

            // Clamped linear lookup over [-_halfRange, +_halfRange]. Inputs outside the domain
            // return the boundary sample (no wrap). v = 0 is bit-exact (index N/2, fraction 0).
            inline sample_t getClampedLinear (sample_t v) const {
                const sample_t idx = sample_t (N) * sample_t (0.5) + v * _scale;
                if (idx <= 0)         return values [0];
                if (idx >= N - 1)     return values [N - 1];
                const uint_fast16_t i = uint_fast16_t (idx);
                const sample_t f = idx - i;
                return values [i] + f * (values [i + 1] - values [i]);
            }

            // Clamped 3-point quadratic lookup. Interpolating (passes through samples), so v = 0
            // stays bit-exact. Inputs outside the domain return the boundary sample; the lowest
            // cell (no left neighbour) falls back to linear.
            inline sample_t getClampedQuadratic (sample_t v) const {
                const sample_t idx = sample_t (N) * sample_t (0.5) + v * _scale;
                if (idx <= 0)         return values [0];
                if (idx >= N - 1)     return values [N - 1];
                const uint_fast16_t i = uint_fast16_t (idx);
                const sample_t f = idx - i;
                if (i == 0)           return values [0] + f * (values [1] - values [0]);
                const sample_t y0 = values [i - 1], y1 = values [i], y2 = values [i + 1];
                const sample_t a1 = sample_t (0.5) * (y2 - y0);
                const sample_t a2 = sample_t (0.5) * (y2 - sample_t (2) * y1 + y0);
                return y1 + f * (a1 + f * a2);
            }
    };


}

