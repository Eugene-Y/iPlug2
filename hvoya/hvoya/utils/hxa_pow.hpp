/*------------------------------------------------------------------------------
 
 HXA7241 General library.
 Harrison Ainsworth / HXA7241 : 2004-2011
 
 http://www.hxa.name/
 
 ------------------------------------------------------------------------------*/

// https://www.hxa.name/articles/content/fast-pow-adjustable_hxa7241_2007.html#supplement
// https://github.com/hxa7241/powfast


#pragma once

#include <cstdint>


namespace hvoya::utils::fast_approx {

    /**
     * Fast approximation to pow, with adjustable precision.
     *
     * Precision can be 0 to 18.
     * Storage is (2 ^ precision) * 4 bytes -- 4B to 1MB
     * For precision 11: mean error < 0.01%, max error < 0.02%, storage 8KB.
     */
    class PowFast {
        
    public:
        explicit PowFast ( uint32_t precision = 11 );
        
        ~PowFast();
    private:
        PowFast ( const PowFast& );
        PowFast& operator= ( const PowFast& );
    public:
        
        /** 2 ^ number. Number must be > -125 and < +128.*/
        float two ( float ) const;
        
        /** e ^ number. Number must be > -87.3ish and < +88.7ish. */
        float e  ( float ) const;
        
        /** 10 ^ number. Number must be > -37.9ish and < +38.5ish. */
        float ten ( float ) const;
        
        /**
         * Get r ^ number.<br/><br/>
         *
         * @logr  logE of radix for power
         * @f     power to apply (beware under/over-flow)
         */
        float r  ( float logr, float f ) const;
        
        uint32_t precision() const;
        
    private:
        uint32_t  precision_m;
        uint32_t* pTable_m;
    };

	
} // ns hvoya::utils::fast_approx
