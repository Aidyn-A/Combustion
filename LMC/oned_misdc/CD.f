
      block data chemdat
      include 'spec.h'
      data traninit / -1 /
      data tranfile / 'tran.asc.chem-H' /
      data TMIN_TRANS / 0.d0 /
      data Pr / 0.7d0 /
      data Sc / 0.7d0 /
      data LeEQ1 / 0 /
      data thickFacTR / 1.d0 /
      data thickFacCH / 1.d0 /
      data max_vode_subcycles / 15000 /
      data min_vode_timestep / 1.e-14 /
      data Pcgs_dvd / -1 /
      data iH2  / -1 /
      data iO2  / -1 /
      data iN2  / -1 /
      data iCH4 / -1 /
      end



      subroutine calcDiffusivity(T, Y, Patm, rhoD, kappa, mu)
      implicit none
      include 'spec.h'
      double precision T, Y(*), Patm
      double precision rhoD(*), kappa, mu

      double precision Ptmp, Dt(maxspec), CPMS(maxspec)
      double precision RHO, Tt, Wavg
      double precision X(maxspec), alpha, l1, l2, cpmix, RWRK
      integer n, IWRK

      if (LeEQ1 .eq. 0) then
         
c     Ensure chem/tran initialized
         if (traninit.lt.0) call initeg()
         
         Ptmp = Patm * P1ATM
         Tt = MAX(T,TMIN_TRANS) 
         
         CALL CKMMWY(Y,IWRK,RWRK,Wavg)
         CALL CKCPMS(Tt,IWRK,RWRK,CPMS)
         CALL CKYTX(Y,IWRK,RWRK,X)
         CALL EGSPAR(Tt,X,Y,CPMS,EGRWRK,EGIWRK)
         CALL EGSV1(Ptmp,Tt,Y,Wavg,EGRWRK,Dt)
         CALL CKRHOY(Ptmp,Tt,Y,IWRK,RWRK,RHO)

         do n=1,Nspec
            rhoD(n) = RHO * Wavg * invmwt(n) * Dt(n)
         end do
         
         alpha = 1.0D0
         CALL EGSL1(alpha, Tt, X, EGRWRK, l1)
         alpha = -1.0D0
         CALL EGSL1(alpha, Tt, X, EGRWRK, l2)
         kappa = .5 * (l1 + l2)
         
         CALL EGSE3(Tt, Y, EGRWRK, mu)

         
         if (thickFacTR.ne.1.d0) then
            
            do n=1,Nspec
               rhoD(n) = rhoD(n) * thickFacTR
            end do
            kappa = kappa * thickFacTR
            
         endif
         
      else

         mu = 1.d1 * 1.85e-5*(T/298.0)**.7
         do n=1,Nspec
            rhoD(n) = mu * thickFacTR / Sc
         enddo
         
         CALL CKCPBS(T,Y,IWRK,RWRK,CPMIX)
         kappa = rhoD(1) * CPMIX * thickFacTR / Pr

      endif
      end

      subroutine initeg
      implicit none
      include 'spec.h'
      double precision RU, RUC, RWRK
      integer lout, IWRK, n, len, offset,i
      integer kname(maxspnml*maxspec)

      if (traninit.lt.0) then
         open(unit=51,status='old',form='formatted',file=tranfile)
         call CNVTTR(51)
         lout = 6
         call EGINI(eg_nodes, lout, eg_IFLAG, eg_ITLS,
     &        EGRWRK, egr, EGIWRK, egi)

c     Other useful things
         call CKRP(IWRK, RWRK, RU, RUC, P1ATM)
         call CKWT(IWRK, RWRK, mwt)         
         do n=1,Nspec
            invmwt(n) = 1.d0 / mwt(n)
         end do

         call cksyms(kname,maxspnml)
         specNameLen = 0
         do n=1,Nspec
            offset = (n-1)*maxspnml+1
            call convStr(kname(offset),maxspnml,specNames(n),len)
            specNamelen = MAX(specNameLen,len)
            if (specNames(n).eq.'H2')  iH2=n
            if (specNames(n).eq.'O2')  iO2=n
            if (specNames(n).eq.'N2')  iN2=n
            if (specNames(n).eq.'CH4') iCH4=n
         enddo
         traninit = 1
      endif
      end

      subroutine CNVTTR (LINKMC)      
      implicit none
      include 'spec.h'
      character*(maxspnml) CDUMMY
      character*16 CFMT, IFMT, LFMT, RFMT
      PARAMETER
     1     (CFMT='(8A16)', IFMT='(10I12)', LFMT='(L8)',
     2     RFMT='(1P,5E24.16)')
      integer MaxOrder, NOrd, NS, LINKEG, LINKMC, IDUMMY, K, NONS,
     &     NONSNS, N
      parameter (MaxOrder = 4, LINKEG=30)
      double precision RDUMMY, PATM
      logical LDUMMY

      double precision WT(maxspec), POLA(maxspec), DIPO(maxspec),
     &     SIGM(maxspec),
     &     EPSI(maxspec), ZROT(maxspec), COFE(maxspec*MaxOrder),
     &     COFL(maxspec*MaxOrder), COFD(maxspec*maxspec*MaxOrder)
      integer LINA(maxspec)
C-----------------------------------------------------------------------
C     CHEMKIN-III TRANSPORT LINKING FILE (Warning: CK I and II different)
C-----------------------------------------------------------------------
      READ (LINKMC,CFMT) CDUMMY
      READ (LINKMC,CFMT) CDUMMY
      READ (LINKMC,CFMT) CDUMMY
      READ (LINKMC,LFMT) LDUMMY
      READ (LINKMC,IFMT) IDUMMY, IDUMMY, NOrd, NS, IDUMMY


      if (NOrd .GT. MaxOrder) then
c         call bl_abort('Polyfit too large for EGLib!')
      end if
c
c     Make sure the transport & chemistry files are consistent.
c
      CALL CKINDX(IDUMMY,RDUMMY,Nelt,Nspec,Nreac,Nfit)
      if (Nelt.gt.maxelts) then
         print *,'Too many elements, increase maxelts'
      endif
      if (Nspec.gt.maxspec) then
         print *,'Too many species, increase maxspec'
      endif
      if (Nreac.gt.maxreac) then
         print *,'Too many reactions, increase maxreac'
      endif

      if (NS .NE. Nspec) then
         print*, 'transport database thinks Nspec = ', NS
         print*, 'chemistry database thinks Nspec = ', Nspec
c         call bl_abort("chemistry & transport are not consistent")
      endif

      NONS = NOrd * NS
      NONSNS = NONS * NS
      READ (LINKMC,RFMT) PATM
      READ (LINKMC,RFMT) (WT(K),   K=1, NS)
      READ (LINKMC,RFMT) (EPSI(K), K=1, NS) 
      READ (LINKMC,RFMT) (SIGM(K), K=1, NS)
      READ (LINKMC,RFMT) (DIPO(K), K=1, NS)
      READ (LINKMC,RFMT) (POLA(K), K=1, NS)
      READ (LINKMC,RFMT) (ZROT(K), K=1, NS) 
      READ (LINKMC,IFMT) (LINA(K), K=1, NS)
      READ (LINKMC,RFMT) (COFL(N), N=1, NONS)
      READ (LINKMC,RFMT) (COFE(N), N=1, NONS)
      READ (LINKMC,RFMT) (COFD(N), N=1, NONSNS)
      rewind(unit=LLINKMC)
C-----------------------------------------------------------------------
C     WRITE EGLIB TRANSPORT LINKING FILE
C-----------------------------------------------------------------------
      OPEN (UNIT=LINKEG,STATUS='unknown',FORM='UNFORMATTED',
     &     FILE='Linkeg')
      WRITE (LINKEG) NS, NOrd, (WT(K), K=1, NS), (EPSI(K), K=1, NS), 
     &                (SIGM(K), K=1, NS), (DIPO(K), K=1, NS), 
     &                (POLA(K), K=1, NS), (ZROT(K), K=1, NS), 
     &                (LINA(K), K=1, NS), (COFE(N), N=1, NONS), 
     &                (COFL(N), N=1, NONS), (COFD(N), N=1, NONSNS)
      CLOSE(UNIT=LINKEG)
      end

      subroutine convStr(codedString,maxLen,string,strLen)
      implicit none
      integer codedString(*), maxLen, k, strLen
      character*(*) string
      do k=1,maxLen
         string(k:k) = ' '
         if (codedString(k) .ne. ICHAR(' ')) then
            string(k:k) = CHAR(codedString(k))
            strLen = k
         endif
      enddo
      end


      subroutine conpFY(N, TIME, Z, ZP, RPAR, IPAR)
      implicit none
      include 'spec.h'
      double precision TIME, Z(maxspec+1), ZP(maxspec+1), RPAR(*)
      integer N, IPAR(*)
      
      double precision RHO, CPB, SUM, H, WDOT, WT, THFAC, Pcgs
      double precision HK(maxspec), WDOTK(maxspec), CONC(maxspec), RWRK
      integer K, IWRK

      Pcgs = Pcgs_dvd
      if (Pcgs.lt.0.d0) then
         print *,'conpFY: Must set P(cgs) before calling vode'
         stop
      endif

      call CKRHOY(Pcgs,Z(1),Z(2),IWRK,RWRK,RHO)
      call CKCPBS(Z(1),Z(2),IWRK,RWRK,CPB)
      call CKYTCP(Pcgs,Z(1),Z(2),IWRK,RWRK,CONC)
      call CKWC(Z(1), CONC, IWRK, RWRK, WDOTK)
      call CKHMS(Z(1), IWRK, RWRK, HK)

      THFAC = 1.d0 / thickFacCH
      SUM = 0.d0
      DO K = 1, Nspec
         H    = HK(K)
         WDOT = WDOTK(K) * THFAC
         WT   = mwt(K)
         ZP(K+1) = WDOT * WT / RHO
         SUM = SUM + H * WDOT * WT
      END DO
      ZP(1) = -SUM / (RHO*CPB)
      END


      subroutine conpJY(NEQ, T, Y, ML, MU, PD, NRPD, RPAR, IPAR)
      implicit none
      integer NEQ, NRPD, ML, MU, IPAR(*)
      double precision T, Y(NEQ), PD(NRPD,NEQ), RPAR(*)
      print *,'Should not be in conpJY'
      stop
      end




      subroutine chemsolve(Ynew, Tnew, Yold, Told, FuncCount, Patm, dt,
     &     diag, do_diag)
      implicit none
      include 'spec.h'

      double precision YJ_SAVE(80)
      logical FIRST
      common /VHACK/ YJ_SAVE, FIRST
      save   /VHACK/

      integer do_diag
      double precision Yold(*), Ynew(*), Told, Tnew, FuncCount
      double precision Patm, dt, diag(*)
   
      integer NEQ, ITOL, IOPT, ITASK, open_vode_failure_file
      parameter (ITOL=1, IOPT=1, ITASK=1)
      double precision RTOL, ATOL(maxspec+1), ATOLEPS, TT1, TT2
      parameter (RTOL=1.0E-8, ATOLEPS=1.0E-8)
      external conpFY, conpJY, open_vode_failure_file
      integer m, MF, ISTATE, lout
      character*(maxspnml) name

      integer nsubchem, nsub, node
      double precision dtloc, weight, TT1save
      double precision Ct(maxspec),Qt(maxreac), scale

      double precision dY(maxspec), Ytemp(maxspec),Yres(maxspec),sum
      double precision zp(maxspec+1)
      logical newJ_triggered, bad_soln


c     DVODE workspace requirements      
      integer dvr, dvi
      parameter (dvr = 22 + 9*(maxspec+1) + 2*(maxspec+1)**2)
      parameter (dvi = 30 + maxspec + 1)
      
      double precision DVRWRK(dvr)
      integer DVIWRK(dvi)

      double precision Z(maxspec+1)
      double precision RPAR, RWRK
      integer IPAR, IWRK

c     IOPT=1 parameter settings for VODE
      DVRWRK(7) = min_vode_timestep
      DVIWRK(6) = max_vode_subcycles

      if (do_diag.eq.1) nsubchem = nchemdiag
c
c     Set pressure in area accessible by conpFY
c
      Pcgs_dvd = Patm * P1atm
      
      MF = 22
      ATOL(1) = ATOLEPS
      TT1 = 0.d0
      TT2 = dt
      if (do_diag.eq.1) then
         nsub = nsubchem
         dtloc = dt/nsubchem
      else
         nsub = 1
         dtloc = dt
      endif
      ISTATE = 1
      NEQ = Nspec + 1


      sum = 0.d0
      do m=1,Nspec
         Ytemp(m) = MAX(Yold(m),0.d0)
         sum = sum+Ytemp(m)
      end do
      if (iN2 .gt. 0) then
         Ytemp(iN2) = Ytemp(iN2)+1.d0-sum
      endif

      Z(1) = Told
      do m=1,Nspec
         Z(1+m) = Ytemp(m)
      end do

c     Always form Jacobian to start
      FIRST = .TRUE.

      if (do_diag.eq.1) then
         FuncCount = 0
         CALL CKYTCP(Pcgs_dvd,Z(1),Z(2),IWRK,RWRK,Ct)
         CALL CKQC(Z(1),Ct,IWRK,RWRK,Qt)
         do m=1,Nreac
            diag(m) = diag(m) + 0.5*dtloc*Qt(m)
         enddo
      endif

      do node = 1,nsub
         if (node.lt.nsub) then
            weight = 1.d0
         else
            weight = 0.5d0
         endif

         TT1save = TT1
         TT2 = TT1 + dtloc

         CALL DVODE
     &        (conpFY, NEQ, Z, TT1, TT2, ITOL, RTOL, ATOL,
     &        ITASK, ISTATE, IOPT, DVRWRK, dvr, DVIWRK,
     &        dvi, conpJY, MF, RPAR, IPAR)

         TT1 = TT2

         if (do_diag.eq.1) then
            CALL CKYTCP(Pcgs_dvd,Z(1),Z(2),IWRK,RWRK,Ct)
            CALL CKQC(Z(1),Ct,IWRK,RWRK,Qt)
            do m=1,Nreac
               diag(m) = diag(m) + weight*dtloc*Qt(m)
            enddo
            FuncCount = FuncCount + DVIWRK(11)
         else
            FuncCount = DVIWRK(11)
         endif

         if (verbose_vode .eq. 1) then
            write(6,*) '......dvode done:'
            write(6,*) ' last successful step size = ',DVRWRK(10)
            write(6,*) '          next step to try = ',DVRWRK(11)
            write(6,*) '   integrated time reached = ',DVRWRK(12)
            write(6,*) '      number of time steps = ',DVIWRK(10)
            write(6,*) '              number of fs = ',DVIWRK(11)
            write(6,*) '              number of Js = ',DVIWRK(12)
            write(6,*) '    method order last used = ',DVIWRK(13)
            write(6,*) '   method order to be used = ',DVIWRK(14)
            write(6,*) '            number of LUDs = ',DVIWRK(18)
            write(6,*) ' number of Newton iterations ',DVIWRK(19)
            write(6,*) ' number of Newton failures = ',DVIWRK(20)
         end if
               
         Tnew = Z(1)
         do m=1,Nspec
            Ynew(m) = Yold(m)+Z(m+1)-Ytemp(m)
         end do

      enddo
      end


      subroutine FORT_TfromHYpt(T,Hin,Y,errMax,NiterMAX,res,Niter)
      implicit none
      include 'spec.h'
      double precision T,Y(*),H,Hin
      double precision TMIN,TMAX,errMAX
      integer NiterMAX,Niter,n,NiterDAMP
      parameter (TMIN=250, TMAX=5000)
      double precision  T0,cp,cv,dH,temp,RoverWbar,Wbar,RU,RUC
      double precision res(0:NiterMAX-1),dT, Htarg
      logical out_of_bounds, converged, soln_bad, stalled
      double precision h300,cp300,h6500,cp6500
      integer ihitlo,ihithi,j,IWRK
      double precision RWRK

      out_of_bounds(temp) = (temp.lt.TMIN) .or. (temp.gt.TMAX)

      NiterDAMP = NiterMAX
      if ((T.GE.TMIN).and.(T.LE.TMAX)) then
         T0 = T
      else
         T0 = 0.5*(TMIN+TMAX)
         T = T0
      end if
      Niter = 0
      dH = 0.d0
      soln_bad = .FALSE.
c     Hin in MKS, convert to CGS
c      Htarg = Hin * 1.d4
      Htarg = Hin
      ihitlo = 0
      ihithi = 0

      CALL CKHBMS(T,Y,IWRK,RWRK,H)
      dH = 2*ABS(H - Htarg)/(1.d0 + ABS(H) + ABS(Htarg))

      res(Niter) = dH
      converged = dH.le.errMAX

      do while ((.not.converged) .and. (.not.soln_bad))

         CALL CKCPBS(T,Y,IWRK,RWRK,cp)
         dT = (Htarg - H)/cp
         if ((Niter.le.NiterDAMP).and.(T+dT.ge.TMAX)) then
            T = TMAX
            ihithi = 1
         else if ((Niter.le.NiterDAMP).and.(T+dT.le.TMIN)) then
            T = TMIN
            ihitlo = 1
         else
            T = T + dT
         end if
         soln_bad = out_of_bounds(T)
         if (soln_bad) then
            Niter = -1
            goto 100
         else
            CALL CKHBMS(T,Y,IWRK,RWRK,H)
            dH = 2*ABS(H - Htarg)/(1.d0 + ABS(H) + ABS(Htarg))
            res(Niter) = dH
            Niter = Niter + 1
         end if
         if (Niter .ge. NiterMAX) then
            Niter = -2
            goto 100
         endif
         converged = (ABS(dH).le.errMAX) .or. (ABS(dT).le.errMAX)

         if ((ihitlo.eq.1).and.(H.gt.Htarg)) then
            T = TMIN
            CALL CKHBMS(T,Y,IWRK,RWRK,h300)
            CALL CKCPBS(T,Y,IWRK,RWRK,cp300)
            T=TMIN+(Htarg-h300)/cp300
            converged = .true.
         endif
         if ((ihithi.eq.1).and.(H.lt.Htarg)) then
            T = TMAX
            CALL CKHBMS(T,Y,IWRK,RWRK,h6500)
            CALL CKCPBS(T,Y,IWRK,RWRK,cp6500)
            T=TMAX+(Htarg-h6500)/cp6500
            converged = .true.
         endif
      end do

      return
c
c     Error condition....dump state and bail out
c
 100  continue

      write(6,997) 'T from (H,Y): failed'
      write(6,997) 'iterations tried = ',Niter
      write(6,998) 'initial T = ',T0
      write(6,998) 'current T = ',T
      write(6,998) 'species mass fracs:'
      do n = 1,Nspec
         write(6,998) '  ',Y(n)
      end do
      write(6,998)
      write(6,998) 'residual:'
      do n = 0,Niter-1
         write(6,998) '  ',res(n)
      end do

 997  format(a,3(i4,a))
 998  format(a,d21.12)
      end
  
      integer function open_vode_failure_file ()
      implicit none
      character*30 name, myproc
      integer lout,i,j,k,idx

c     Hardwire the unit number to 26 for the moment
      lout = 26 
c      call bl_pd_myproc(i)
      i = 0
      write(myproc, *) i
      idx = 1 
      do j = 1, 30
         if (myproc(j:j) .ne. ' ') then
            idx = j
            goto 1
         end if 
      end do
 1    continue
      do k = 30, j+1, -1
         if (myproc(k:k) .ne. ' ') then
            goto 2
         end if
      end do
 2    continue
      write(name, '(2a)') 'vode.failed.', myproc(idx:k)
c      write(name, '(2a)') 'vode.failed.', myproc(idx:30)
      open(unit=lout, file=name, form='formatted', status='replace')
      open_vode_failure_file = lout
      end
