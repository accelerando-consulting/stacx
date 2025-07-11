#ifndef _TRAIT_POLYNOMIAL_H
#define _TRAIT_POLYNOMIAL_H

#include <stdarg.h>
#include <malloc.h>
#include <math.h>

class Polynomial: public Debuggable
{
protected:
  int polynomial_degree=-1;
  double *polynomial_coefficients=NULL;

  void polynomial_allocate(int degree)
  {
    if (polynomial_coefficients && (polynomial_degree==degree)) {
      // storage is already good
      return;
    }
    if (polynomial_coefficients) {
      free(polynomial_coefficients);
    }
    polynomial_degree = degree;
    polynomial_coefficients = (double*)malloc((degree+1) * sizeof(double));
  }

public:
  Polynomial(String name, int l=L_USE_DEFAULT)
    : Debuggable(name, l)
  {
  }

  void polynomial_setup(int degree, ...)
  {
    va_list ap;
    va_start(ap, degree);
    polynomial_allocate(degree);
    for (int n=degree; n>=0; n--) {
      this->polynomial_coefficients[n] = va_arg(ap, double);
      LEAF_NOTICE(" coefficient %d: %lf", n, this->polynomial_coefficients[n]);
    }
    va_end(ap);
  }

  void polynomial_vsetup(int degree, double *coefficients)
  {
    polynomial_allocate(degree);
    for (int n=degree; n>=0; n--) {
      this->polynomial_coefficients[n] = coefficients[n];
      LEAF_NOTICE(" coefficient %d: %lf", n, this->polynomial_coefficients[n]);
    }
  }

  double polynomial_compute(double x)
  {
    double y = 0;
    LEAF_INFO(" x=%lf", x);
    for (int n=this->polynomial_degree; n>=0;n--) {
      if (this->polynomial_coefficients[n]==0) continue;
      y += polynomial_coefficients[n]*pow(x,n);
      LEAF_INFO("  y += %lf * x^^%d => %lf", polynomial_coefficients[n], n, y);
    }
    return y;
  }
};


#endif
// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
