
#include <winstd.H>
#include <Laplacian.H>
#include <LP_F.H>

Laplacian::Laplacian (const BndryData& bd,
                      Real             _h)
    :
    LinOp(bd,_h) {}

Laplacian::~Laplacian() {}

Real
Laplacian::norm (int nm, int level, const bool local)
{
  switch ( nm )
    {
    case 0:
      return 8.0/(h[level][0]*h[level][0]);
    }
  BoxLib::Error("Bad Laplacian::norm");
  return -1.0;
}

void
Laplacian::compFlux (D_DECL(MultiFab &xflux, MultiFab &yflux, MultiFab &zflux),
		     MultiFab& in, const BC_Mode& bc_mode, int src_comp, int dst_comp)
{
    int level = 0;
    int num_comp = 1;

    BL_ASSERT(in.nComp() > src_comp);
    D_TERM(BL_ASSERT(xflux.nComp()>dst_comp);,
           BL_ASSERT(yflux.nComp()>dst_comp);,
           BL_ASSERT(zflux.nComp()>dst_comp););

    applyBC(in,src_comp,num_comp,level,bc_mode);
    int nc = in.nComp();

    for (MFIter inmfi(in); inmfi.isValid(); ++inmfi)
    {
        FORT_FLUX(in[inmfi].dataPtr(src_comp),
		  ARLIM(in[inmfi].loVect()), ARLIM(in[inmfi].hiVect()),
		  inmfi.validbox().loVect(), inmfi.validbox().hiVect(), &nc,
		  h[level],
		  xflux[inmfi].dataPtr(dst_comp),
		  ARLIM(xflux[inmfi].loVect()), ARLIM(xflux[inmfi].hiVect())
#if (BL_SPACEDIM >= 2)
		  ,yflux[inmfi].dataPtr(dst_comp),
		  ARLIM(yflux[inmfi].loVect()), ARLIM(yflux[inmfi].hiVect())
#endif
#if (BL_SPACEDIM == 3)
		  ,zflux[inmfi].dataPtr(dst_comp),
		  ARLIM(zflux[inmfi].loVect()), ARLIM(zflux[inmfi].hiVect())
#endif
		  );
    }
}

void
Laplacian::Fsmooth (MultiFab&       solnL,
                    const MultiFab& rhsL,
                    int             level,
                    int             redBlackFlag)
{
    OrientationIter oitr;
    const FabSet& f0 = (*undrrelxr[level])[oitr()]; oitr++;
    const FabSet& f1 = (*undrrelxr[level])[oitr()]; oitr++;
    const FabSet& f2 = (*undrrelxr[level])[oitr()]; oitr++;
    const FabSet& f3 = (*undrrelxr[level])[oitr()]; oitr++;
#if (BL_SPACEDIM > 2)
    const FabSet& f4 = (*undrrelxr[level])[oitr()]; oitr++;
    const FabSet& f5 = (*undrrelxr[level])[oitr()]; oitr++;
#endif
    int nc = rhsL.nComp();

    for (MFIter solnLmfi(solnL); solnLmfi.isValid(); ++solnLmfi)
    {
        oitr.rewind();
        int gn = solnLmfi.index();
        const Mask& m0 = *maskvals[level][gn][oitr()]; oitr++;
        const Mask& m1 = *maskvals[level][gn][oitr()]; oitr++;
        const Mask& m2 = *maskvals[level][gn][oitr()]; oitr++;
        const Mask& m3 = *maskvals[level][gn][oitr()]; oitr++;
#if (BL_SPACEDIM > 2 )
        const Mask& m4 = *maskvals[level][gn][oitr()]; oitr++;
        const Mask& m5 = *maskvals[level][gn][oitr()]; oitr++;
#endif

#if (BL_SPACEDIM == 2)
        FORT_GSRB(
            solnL[solnLmfi].dataPtr(), 
            ARLIM(solnL[solnLmfi].loVect()),ARLIM(solnL[solnLmfi].hiVect()),
            rhsL[solnLmfi].dataPtr(), 
            ARLIM(rhsL[solnLmfi].loVect()), ARLIM(rhsL[solnLmfi].hiVect()),
            f0[solnLmfi].dataPtr(), 
            ARLIM(f0[solnLmfi].loVect()), ARLIM(f0[solnLmfi].hiVect()),
            m0.dataPtr(), 
            ARLIM(m0.loVect()), ARLIM(m0.hiVect()),
            f1[solnLmfi].dataPtr(), 
            ARLIM(f1[solnLmfi].loVect()), ARLIM(f1[solnLmfi].hiVect()),
            m1.dataPtr(), 
            ARLIM(m1.loVect()), ARLIM(m1.hiVect()),
            f2[solnLmfi].dataPtr(), 
            ARLIM(f2[solnLmfi].loVect()), ARLIM(f2[solnLmfi].hiVect()),
            m2.dataPtr(), 
            ARLIM(m2.loVect()), ARLIM(m2.hiVect()),
            f3[solnLmfi].dataPtr(), 
            ARLIM(f3[solnLmfi].loVect()), ARLIM(f3[solnLmfi].hiVect()),
            m3.dataPtr(), 
            ARLIM(m3.loVect()), ARLIM(m3.hiVect()),
            solnLmfi.validbox().loVect(), solnLmfi.validbox().hiVect(), &nc,
            h[level], &redBlackFlag);
#endif

#if (BL_SPACEDIM == 3)
        FORT_GSRB(
            solnL[solnLmfi].dataPtr(), 
            ARLIM(solnL[solnLmfi].loVect()),ARLIM(solnL[solnLmfi].hiVect()),
            rhsL[solnLmfi].dataPtr(), 
            ARLIM(rhsL[solnLmfi].loVect()), ARLIM(rhsL[solnLmfi].hiVect()),
            f0[solnLmfi].dataPtr(), 
            ARLIM(f0[solnLmfi].loVect()), ARLIM(f0[solnLmfi].hiVect()),
            m0.dataPtr(), 
            ARLIM(m0.loVect()), ARLIM(m0.hiVect()),
            f1[solnLmfi].dataPtr(), 
            ARLIM(f1[solnLmfi].loVect()), ARLIM(f1[solnLmfi].hiVect()),
            m1.dataPtr(), 
            ARLIM(m1.loVect()), ARLIM(m1.hiVect()),
            f2[solnLmfi].dataPtr(), 
            ARLIM(f2[solnLmfi].loVect()), ARLIM(f2[solnLmfi].hiVect()),
            m2.dataPtr(), 
            ARLIM(m2.loVect()), ARLIM(m2.hiVect()),
            f3[solnLmfi].dataPtr(), 
            ARLIM(f3[solnLmfi].loVect()), ARLIM(f3[solnLmfi].hiVect()),
            m3.dataPtr(), 
            ARLIM(m3.loVect()), ARLIM(m3.hiVect()),
            f4[solnLmfi].dataPtr(), 
            ARLIM(f4[solnLmfi].loVect()), ARLIM(f4[solnLmfi].hiVect()),
            m4.dataPtr(), 
            ARLIM(m4.loVect()), ARLIM(m4.hiVect()),
            f5[solnLmfi].dataPtr(), 
            ARLIM(f5[solnLmfi].loVect()), ARLIM(f5[solnLmfi].hiVect()),
            m5.dataPtr(), 
            ARLIM(m5.loVect()), ARLIM(m5.hiVect()),
            solnLmfi.validbox().loVect(), solnLmfi.validbox().hiVect(), &nc,
            h[level], &redBlackFlag);
#endif
    }
}

void
Laplacian::Fsmooth_jacobi (MultiFab&       solnL,
                           const MultiFab& rhsL,
                           int            level)
{
}

void
Laplacian::Fapply (MultiFab&       y,
                   const MultiFab& x,
                   int             level)
{
    int nc = y.nComp();

    for (MFIter ymfi(y); ymfi.isValid(); ++ymfi)
    {
        FORT_ADOTX(y[ymfi].dataPtr(), 
                   ARLIM(y[ymfi].loVect()), ARLIM(y[ymfi].hiVect()),
                   x[ymfi].dataPtr(), 
                   ARLIM(x[ymfi].loVect()), ARLIM(x[ymfi].hiVect()),
                   ymfi.validbox().loVect(), ymfi.validbox().hiVect(), &nc,
                   h[level]);
    }
}
