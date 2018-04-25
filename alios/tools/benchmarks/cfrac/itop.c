#include "pdefs.h"
#include "pcvt.h"
#include "precision.h"

/*
 * Integer to Precision
 */
precision itop(i)
   register int i;
{
   register digitPtr  uPtr;
   register precision u = palloc(INTSIZE);

   if (u == pUndef) return u;
   u->sign = (i<0);
   if (u->sign) i = -i;
   uPtr	      = u->value;
   do {
      *uPtr++ = modBase(i);
      i	      = divBase(i);
   } while (i != 0);

   u->size = (uPtr - u->value);			/* normalize */
   return presult(u);
}
