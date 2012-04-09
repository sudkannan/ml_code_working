/* 
 * Copyright (C) 2002, 2003 Laird Breyer
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * Author:   Laird Breyer <laird@lbreyer.com>
 */

#include <math.h>
#include "util.h"

/* returns the logarithm of a Poisson distribution 
   (calculations based on Stirling's formula) */
double log_poisson(int k, double lambda) {

  return (double)k * (log(lambda) -log((double)k) + 1.0) - 
    log(2.0 * M_PI * (double)k ) / 2.0 - lambda;

}

/* returns the probability that a chi squared with df
   degrees of freedom is less than or equal to x */
double chi2_cdf(double df, double x) {
  /* to be implemented */
  return 1.0 - igamc(df/2.0, x/2.0);
}

double gamma_tail(double a, double b, double x) {
  /* don't call igamc with extreme numbers, it can exit with an error */
  return igamc(a, x/b);
}

double normal_cdf(double x) {
  return ndtr(x);
}
