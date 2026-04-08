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
    };


}

