/* 2D frequency-domain migration with space-lag imaging condition. */
/*
  Copyright (C) 2012 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <rsf.h>
#include <umfpack.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "fdprep.h"

int main(int argc, char* argv[])
{
    bool verb, save, load;
    int npw, npml, pad1, pad2, n1, n2; 
    int ih, nh, is, ns, iw, nw, i, j;
    SuiteSparse_long n, nz, *Ti, *Tj;
    float eps, d1, d2, **vel, ***image, dw, ow;
    double omega, *Tx, *Tz;
    SuiteSparse_long status, *Ap, *Ai, *Map;
    double *Ax, *Az, *Xx, *Xz, *Bx, *Bz;
    void *Symbolic, *Numeric;
    double Control[UMFPACK_CONTROL], Info[UMFPACK_INFO];
    sf_complex ***srce, ***recv;
    char *datapath, *insert, *append;
    size_t srclen, inslen;
    sf_file in, out, source, data, us, ur;

    sf_init(argc,argv);
    in  = sf_input("in");
    out = sf_output("out");

    if (!sf_getbool("verb",&verb)) verb=false;
    /* verbosity flag */

    if (!sf_getbool("save",&save)) save=false;
    /* save LU */

    if (!sf_getbool("load",&load)) load=false;
    /* load LU */

    if (save || load) {
	datapath = sf_histstring(in,"in");
	srclen = strlen(datapath);
	insert = sf_charalloc(6);
    } else {
	datapath = NULL;
	insert = NULL;
	append = NULL;
    }

    if (!sf_getint("nh",&nh)) nh=0;
    /* horizontal space-lag */

    if (!sf_getint("npw",&npw)) npw=6;
    /* number of points per wave-length */

    if (!sf_getfloat("eps",&eps)) eps=0.01;
    /* epsilon for PML */

    /* read model */
    if (!sf_histint(in,"n1",&n1)) sf_error("No n1= in input.");
    if (!sf_histint(in,"n2",&n2)) sf_error("No n2= in input.");

    if (!sf_histfloat(in,"d1",&d1)) sf_error("No d1= in input.");
    if (!sf_histfloat(in,"d2",&d2)) sf_error("No d2= in input.");

    vel = sf_floatalloc2(n1,n2);
    sf_floatread(vel[0],n1*n2,in);

    /* read source */
    if (NULL == sf_getstring("source"))
	sf_error("Need source=");
    source = sf_input("source");

    if (!sf_histint(source,"n3",&ns)) sf_error("No ns=.");
    if (!sf_histint(source,"n4",&nw)) sf_error("No nw=.");
    if (!sf_histfloat(source,"d4",&dw)) sf_error("No dw=.");
    if (!sf_histfloat(source,"o4",&ow)) sf_error("No ow=.");

    srce = sf_complexalloc3(n1,n2,ns);

    /* read receiver */
    if (NULL == sf_getstring("data"))
	sf_error("Need data=");
    data = sf_input("data");

    recv = sf_complexalloc3(n1,n2,ns);

    /* write output header */
    sf_putint(out,"n3",2*nh+1);
    sf_putfloat(out,"d3",d2);
    sf_putfloat(out,"o3",(float) -nh*d2);

    if (NULL != sf_getstring("us")) {
	/* output source wavefield */
	us = sf_output("us");
	
	sf_settype(us,SF_COMPLEX);
	sf_putint(us,"n3",ns);
	sf_putstring(us,"label3","Shot");
	sf_putstring(us,"unit3","");
	sf_putint(us,"n4",nw);
	sf_putfloat(us,"d4",dw);
	sf_putfloat(us,"o4",ow);
	sf_putstring(us,"label4","Frequency");
	sf_putstring(us,"unit4","Hz");
    } else {
	us = NULL;
    }

    if (NULL != sf_getstring("ur")) {
	/* output receiver wavefield */
	ur = sf_output("ur");
	
	sf_settype(ur,SF_COMPLEX);
	sf_putint(ur,"n3",ns);
	sf_putstring(ur,"label3","Shot");
	sf_putstring(ur,"unit3","");
	sf_putint(ur,"n4",nw);
	sf_putfloat(ur,"d4",dw);
	sf_putfloat(ur,"o4",ow);
	sf_putstring(ur,"label4","Frequency");
	sf_putstring(ur,"unit4","Hz");
    } else {
	ur = NULL;
    }

    /* allocate temporary memory */
    npml = npw*2;
    pad1 = n1+2*npml;
    pad2 = n2+2*npml;

    n = (pad1-2)*(pad2-2);
    nz = 5*(pad1-2)*(pad2-2)-2*(pad1-4)-2*(pad2-4)-8;

    if (!load) {
	Ti = (SuiteSparse_long*) sf_alloc(nz,sizeof(SuiteSparse_long));
	Tj = (SuiteSparse_long*) sf_alloc(nz,sizeof(SuiteSparse_long));
	Tx = (double*) sf_alloc(nz,sizeof(double));
	Tz = (double*) sf_alloc(nz,sizeof(double));
	
	Ap = (SuiteSparse_long*) sf_alloc(n+1,sizeof(SuiteSparse_long));
	Ai = (SuiteSparse_long*) sf_alloc(nz,sizeof(SuiteSparse_long));
	Map = (SuiteSparse_long*) sf_alloc(nz,sizeof(SuiteSparse_long));
	
	Ax = (double*) sf_alloc(nz,sizeof(double));
	Az = (double*) sf_alloc(nz,sizeof(double));
    } else {
	Ti = NULL; Tj = NULL; Tx = NULL; Tz = NULL; 
	Ap = NULL; Ai = NULL; Map = NULL; Ax = NULL; Az = NULL;
    }

    Bx = (double*) sf_alloc(n,sizeof(double));
    Bz = (double*) sf_alloc(n,sizeof(double));
    Xx = (double*) sf_alloc(n,sizeof(double));
    Xz = (double*) sf_alloc(n,sizeof(double));

    image = sf_floatalloc3(n1,n2,2*nh+1);

    /* turn off iterative refinement */
    umfpack_zl_defaults (Control);
    Control [UMFPACK_IRSTEP] = 0;

    /* loop over frequency */
    for (iw=0; iw < nw; iw++) {
	omega = (double) 2.*SF_PI*(ow+iw*dw);

	if (verb) sf_warning("Frequency %d of %d.",iw+1,nw);

	/* LU file (append _lu* after velocity file) */
	if (save || load) {
	    sprintf(insert,"_lu%d",iw);
	    inslen = strlen(insert);
	    
	    append = malloc(srclen+inslen+1);

	    memcpy(append,datapath,srclen-5);
	    memcpy(append+srclen-5,insert,inslen);
	    memcpy(append+srclen-5+inslen,datapath+srclen-5,5+1);
	}

	if (!load) {
	    /* assemble matrix */
	    fdprep(omega, eps, 
		   n1, n2, d1, d2, vel,
		   npml, pad1, pad2, n, nz, 
		   Ti, Tj, Tx, Tz);
	    
	    status = umfpack_zl_triplet_to_col (n, n, nz, 
						Ti, Tj, Tx, Tz, 
						Ap, Ai, Ax, Az, Map);
	    
	    /* LU */
	    status = umfpack_zl_symbolic (n, n, 
					  Ap, Ai, Ax, Az, 
					  &Symbolic, Control, Info);
	    
	    status = umfpack_zl_numeric (Ap, Ai, Ax, Az, 
					 Symbolic, &Numeric, 
					 Control, Info);

	    /* save Numeric */
	    if (save) status = umfpack_zl_save_numeric (Numeric, append);
	} else {
	    /* load Numeric */
	    status = umfpack_zl_load_numeric (&Numeric, append);
	}
	
	if (save || load) free(append);

	/* loop over shots */
	for (is=0; is < ns; is++) {

	    /* source wavefield */
	    sf_complexread(srce[is][0],n1*n2,source);

	    fdpad(npml,pad1,pad2, srce[is],Bx,Bz);

	    status = umfpack_zl_solve (UMFPACK_A, 
				       NULL, NULL, NULL, NULL, 
				       Xx, Xz, Bx, Bz, 
				       Numeric, Control, Info);

	    fdcut(npml,pad1,pad2, srce[is],Xx,Xz);

	    /* receiver wavefield */
	    sf_complexread(recv[is][0],n1*n2,data);

	    fdpad(npml,pad1,pad2, recv[is],Bx,Bz);

	    status = umfpack_zl_solve (UMFPACK_At, 
				       NULL, NULL, NULL, NULL, 
				       Xx, Xz, Bx, Bz, 
				       Numeric, Control, Info);

	    fdcut(npml,pad1,pad2, recv[is],Xx,Xz);
	}

	if (us != NULL) sf_complexwrite(srce[0][0],n1*n2*ns,us);
	if (ur != NULL) sf_complexwrite(recv[0][0],n1*n2*ns,ur);

	/* imaging condition */
#ifdef _OPENMP    
#pragma omp parallel for private(is,j,i)
#endif
	for (ih=-nh; ih < nh+1; ih++) {
	    for (is=0; is < ns; is++) {
		for (j=0; j < n2; j++) {
		    for (i=0; i < n1; i++) {
			if (j-abs(ih) >= 0 && j+abs(ih) < n2) {
			    image[ih+nh][j][i] += creal(conjf(srce[is][j-ih][i])*recv[is][j+ih][i]);
			}
		    }
		}
	    }
	}
	
	if (!load) (void) umfpack_zl_free_symbolic (&Symbolic);
	(void) umfpack_zl_free_numeric (&Numeric);
    }
    
    sf_floatwrite(image[0][0],n1*n2*(2*nh+1),out);
    
    exit(0);
}
