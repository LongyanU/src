#include <stdio.h>

#include <rsf.h>

#include "chain2dweights.h"
#include "fft2.h"

int main(void) {
    double dot1[2], dot2[2];
    //int n=16, nk, i;
    int n, nz, nx, nz1, nx2, nk, i;
    float *w, *wf;
    nz = 6;
    nx = 5;    
    nk = fft2_init(false, 1, nx, nz, &nx2, &nz1); 

    n = nz*nx;
    w = sf_floatalloc(n);
    wf = sf_floatalloc(nk);

    for (i=0; i < n; i++) {
	w[i] = 1.1 + (i+1.0)/10.0;
    }

    for (i=0; i < nk; i++) {
     wf[i] = 1.2+(i+1.0)/10.0;
    }
    
    chain2dweights_init(nz,nx,nk,nz1,nx2,w,wf);

    sf_dot_test(chain2dweights_lop, n, n, dot1, dot2);
    
    chain2dweights_close();
    printf ("%12.8f ? %12.8f\n",dot1[0],dot1[1]);
    printf ("%12.8f ? %12.8f\n",dot2[0],dot2[1]);

    exit(0);
}
