/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: verify_measurements.c                                               */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

void VerifyMeasurementPromise(struct Promise *pp)

{ struct Attributes a;
 
a = GetMeasurementAttributes(pp);

if (!CheckMeasureSanity(a,pp))
   {
   return;
   }

PromiseBanner(pp);
VerifyMeasurement(a,pp);
}

/*****************************************************************************/

int CheckMeasureSanity(struct Attributes a,struct Promise *pp)

{ int retval = true;
 
if (!IsAbsPath(pp->promiser))
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"The promiser \"%s\" of a measurement was not an absolute path",pp->promiser);
   PromiseRef(cf_error,pp);
   retval = false;
   }

if (!a.measure.data_type)
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"The promiser \"%s\" did not specify a data type\n");
   PromiseRef(cf_error,pp);
   retval = false;   
   }
else
   {
   if (strcmp(a.measure.history_type,"weekly") == 0)
      {
      switch (a.measure.data_type)
         {
         case cf_counter:
         case cf_str:
         case cf_int:
         case cf_real:

             break;

         default:
             cfPS(cf_error,CF_INTERPT,"",pp,a,"The promiser \"%s\" cannot have history type weekly as it is not a number\n");
             PromiseRef(cf_error,pp);
             retval = false;                
             break;
         }
      }
   }

if (a.measure.select_line_matching && a.measure.select_line_number != CF_NOINT)
   {
   cfPS(cf_error,CF_INTERPT,"",pp,a,"The promiser \"%s\" cannot select both a line by pattern and by number\n");
   PromiseRef(cf_error,pp);
   retval = false;                
   }

if (!a.measure.extraction_regex)
   {
   CfOut(cf_verbose,"","No extraction regex, so assuming whole line is the value");
   }

return retval;
}
