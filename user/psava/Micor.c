/* Interferometric cross-correlation of time series (zero-lag output) */
/*
  Copyright (C) 2006 Colorado School of Mines
  
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

#ifdef _OPENMP
#include <omp.h>
#endif

int main(int argc, char* argv[])
{
    bool verb;
    int  version;

    sf_file Fi,Fs,Fr;    /* I/O files */
    sf_axis at,az,ax,aa; /* cube axes */

    int     nt,nz,nx, nhz,nhx,nht, nbuf;
    int     it,iz,ix, ihz,ihx,iht, ibuf;

    float **ii=NULL, ***us=NULL,***ur=NULL; /* arrays */

    int ompchunk; 

    int lox,hix;
    int loz,hiz;
    int lot,hit;

/*------------------------------------------------------------*/

    /* init RSF */
    sf_init(argc,argv);

    if(! sf_getint("ompchunk",&ompchunk)) ompchunk=1;  /* OpenMP data chunk size */
    if(! sf_getbool("verb",&verb)) verb=false;         /* verbosity flag */
    if(! sf_getint("nbuf",&nbuf)) nbuf=1;              /* buffer size */
    if(! sf_getint("version",&version)) version=0;     /* I.C. version (see paper) */

    if(! sf_getint("nhz",&nhz)) nhz=0;
    if(! sf_getint("nhx",&nhx)) nhx=0;
    if(! sf_getint("nht",&nht)) nht=1;

    Fs = sf_input ("in" ); /*   source wavefield */
    Fr = sf_input ("ur" ); /* receiver wavefield */
    Fi = sf_output("out"); /* image */

    /* read axes */
    az=sf_iaxa(Fs,1); sf_setlabel(az,"z"); if(verb) sf_raxa(az); /* depth */
    ax=sf_iaxa(Fs,2); sf_setlabel(ax,"x"); if(verb) sf_raxa(ax); /* position */
    at=sf_iaxa(Fs,3); sf_setlabel(at,"t"); if(verb) sf_raxa(at); /* time */

    /* set output axes */
    aa=sf_maxa(1,0,1); 
    sf_oaxa(Fi,aa,3);

    nz = sf_n(az);
    nx = sf_n(ax);
    nt = sf_n(at);

    nbuf = SF_MIN(nbuf,nt);

    if(version>0) nbuf=nt;

    /* allocate work arrays */
    us=sf_floatalloc3(nz,nx,nbuf);
    ur=sf_floatalloc3(nz,nx,nbuf);
    ii=sf_floatalloc2(nz,nx);

    /* init output */
    for (iz=0; iz<nz; iz++) {
	for (ix=0; ix<nx; ix++) {
	    ii[ix][iz]=0;
	}
    }

    for (; nt > 0; nt -= nbuf) {
	if (nbuf > nt) nbuf=nt;
	if(verb) sf_warning("nsiz=%ld nbuf=%ld",nt,nbuf);
	
	sf_floatread(us[0][0],nz*nx*nbuf,Fs);
	sf_floatread(ur[0][0],nz*nx*nbuf,Fr);
	switch (version){
	    case 1:
		for(        iht=-nht; iht<nht+1; iht++) { lot=SF_ABS(iht); hit=nbuf-SF_ABS(iht);
		    for(    ihx=-nhx; ihx<nhx+1; ihx++) { lox=SF_ABS(ihx); hix=nx  -SF_ABS(ihx);
			for(ihz=-nhz; ihz<nhz+1; ihz++) { loz=SF_ABS(ihz); hiz=nz  -SF_ABS(ihz);
			    for(        it=lot; it<hit; it++) {
				for(    ix=lox; ix<hix; ix++) {
				    for(iz=loz; iz<hiz; iz++) {
					ii[ix][iz] += us[it-iht][ix-ihx][iz-ihz] 
					    *         ur[it+iht][ix+ihx][iz+ihz];
				    } // nz
				} // nx
			    } // nbuf
			} // nhz
		    } // nhx
		} // nht
		break;
	    case 0:
	    default:
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic,ompchunk) private(ibuf,iz,ix,ihz,ihx) shared(nbuf,nz,nx,ii,us,ur)
#endif
		for(ibuf=0; ibuf<nbuf; ibuf++) {
		    for(    ihx=-nhx; ihx<nhx+1; ihx++) { lox=SF_ABS(ihx); hix=nx  -SF_ABS(ihx);
			for(ihz=-nhz; ihz<nhz+1; ihz++) { loz=SF_ABS(ihz); hiz=nz  -SF_ABS(ihz);
			    for(    ix=lox; ix<hix; ix++) {
				for(iz=loz; iz<hiz; iz++) {
				    ii[ix][iz] += us[ibuf][ix-ihx][iz-ihz] 
					*         ur[ibuf][ix+ihx][iz+ihz];
				} // nz
			    } // nx
			} // nhz
		    } // nhx
		} // nbuf
		break;
	}

    } // nt
    
    sf_floatwrite(ii[0],nz*nx,Fi);
    
    exit (0);
}
