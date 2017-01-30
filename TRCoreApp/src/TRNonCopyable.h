/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TRANSREC_NON_COPYABLE_H
#define TRANSREC_NON_COPYABLE_H

/**
 * Class with private unimplemented copy constructor
 * and copy assignment. Inheriting this makes a class
 * non-copyable and non-copy-assignable.
 */
class TRNonCopyable
{
public:
    inline TRNonCopyable ()
    {
    }
    
private:
    TRNonCopyable (TRNonCopyable const &);
    TRNonCopyable & operator= (TRNonCopyable const &);
};

#endif
