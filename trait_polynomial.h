#ifndef _TRAIT_POLYNOMIAL_H
#define _TRAIT_POLYNOMIAL_H

#include <stdarg.h>
#include <malloc.h>
#include <math.h>

class Polynomial
{
protected:
  int polynomial_degree=0;
  double *polynomial_coefficients=NULL;
  
  void polynomial_setup(int degree, ...) 
  {
    va_list ap;
    va_start(ap, degree);
    this->polynomial_degree = degree;
    this->polynomial_coefficients = (double*)malloc((degree+1) * sizeof(double));
    for (int n=degree; n>=0; n--) {
      this->polynomial_coefficients[n] = va_arg(ap, double);
    }
    va_end(ap);
  }
  
  double polynomial_compute(double x) 
  {
    double y = 0;
    for (int n=this->polynomial_degree; n>=0;n--) {
      if (this->polynomial_coefficients[n]==0) continue;
      y += polynomial_coefficients[n]*pow(x,n);
    }
    return y;
  }
};


#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
