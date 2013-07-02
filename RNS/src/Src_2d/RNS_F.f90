
subroutine rns_dudt (lo, hi, &
     U, U_l1, U_l2, U_h1, U_h2, &
     dUdt, Ut_l1, Ut_l2, Ut_h1, Ut_h2, &
     dx)
  use meth_params_module, only : NVAR, gravity, URHO, UMY, UEDEN
  use hypterm_module, only : hypterm
  use difterm_module, only : difterm
  implicit none

  integer, intent(in) :: lo(2), hi(2)
  integer, intent(in) ::  U_l1,  U_h1,  U_l2,  U_h2
  integer, intent(in) :: Ut_l1, Ut_h1, Ut_l2, Ut_h2
  double precision, intent(in)    ::    U( U_l1: U_h1, U_l2: U_h2,NVAR)
  double precision, intent(inout) :: dUdt(Ut_l1:Ut_h1,Ut_l2:Ut_h2,NVAR)
  double precision, intent(in) :: dx(2)

  integer :: Ulo(2), Uhi(2), i, j, n
  double precision :: dxinv(2)
  double precision, allocatable :: fx(:,:,:), fy(:,:,:)

  dxinv(1) = 1.d0/dx(1)
  dxinv(2) = 1.d0/dx(2)
  
  Ulo(1) = U_l1
  Ulo(2) = U_l2
  Uhi(1) = U_h1
  Uhi(2) = U_h2

  allocate(fx(lo(1):hi(1)+1,lo(2):hi(2)  ,NVAR))
  allocate(fy(lo(1):hi(1)  ,lo(2):hi(2)+1,NVAR))

  call hypterm(lo,hi,U,Ulo,Uhi,fx,fy)

  do n=1, NVAR
     do j=lo(2),hi(2)
        do i=lo(1),hi(1)
           dUdt(i,j,n) = dxinv(1)*(fx(i,j,n)-fx(i+1,j,n)) &
                +        dxinv(2)*(fy(i,j,n)-fy(i,j+1,n))
        end do
     end do
  end do

!xxxxx  call difterm(lo,hi,U,Ulo,Uhi,fx,fy,dxinv)

  deallocate(fx, fy)

  if (gravity .ne. 0.d0) then
     do j=lo(2),hi(2)
        do i=lo(1),hi(1)
           dUdt(i,j,UMY  ) = dUdt(i,j,UMY  ) + U(i,j,URHO)*gravity
           dUdt(i,j,UEDEN) = dUdt(i,j,UEDEN) + U(i,j,UMY )*gravity
        end do
     end do
  end if

end subroutine rns_dudt

! :::
! ::: ------------------------------------------------------------------
! :::

subroutine rns_advchem(lo,hi,U,U_l1,U_l2,U_h1,U_h2,dt)
  use meth_params_module, only : NVAR
  use chemterm_module, only : chemterm
  implicit none
  integer, intent(in) :: lo(2), hi(2)
  integer, intent(in) ::  U_l1, U_l2, U_h1, U_h2
  double precision, intent(inout) :: U(U_l1:U_h1,U_l2:U_h2,NVAR)
  double precision, intent(in) :: dt

  integer :: Ulo(2), Uhi(2)

  Ulo(1) = U_l1
  Ulo(2) = U_l2
  Uhi(1) = U_h1
  Uhi(2) = U_h2
  call chemterm(lo, hi, U, Ulo, Uhi, dt)
end subroutine rns_advchem

! :::
! ::: ------------------------------------------------------------------
! :::

subroutine rns_compute_temp(lo,hi,U,U_l1,U_l2,U_h1,U_h2)
  use meth_params_module, only : NVAR, URHO, UMX, UMY, UEDEN, UTEMP, UFS, NSPEC
  use eos_module, only : eos_get_T
  implicit none
  
  integer, intent(in) :: lo(2), hi(2)
  integer, intent(in) :: U_l1, U_l2, U_h1, U_h2
  double precision, intent(inout) :: U(U_l1:U_h1,U_l2:U_h2,NVAR)

  integer :: i, j
  double precision :: rhoInv, e, vx, vy, Y(NSPEC)

  do j=lo(2),hi(2)
  do i=lo(1),hi(1)
     rhoInv = 1.0d0/U(i,j,URHO)

     vx = U(i,j,UMX)*rhoInv     
     vy = U(i,j,UMY)*rhoInv     
     e  = U(i,j,UEDEN)*rhoInv - 0.5d0*(vx**2+vy**2)

     Y = U(i,j,UFS:UFS+NSPEC-1)*rhoInv

     call eos_get_T(U(i,j,UTEMP), e, Y)
  end do
  end do
end subroutine rns_compute_temp

! :::
! ::: ------------------------------------------------------------------
! :::

subroutine rns_enforce_consistent_Y(lo,hi,U,U_l1,U_l2,U_h1,U_h2)
  use meth_params_module, only : NVAR, URHO, UFS, NSPEC
  implicit none
  
  integer, intent(in) :: lo(2), hi(2)
  integer, intent(in) :: U_l1, U_l2, U_h1, U_h2
  double precision, intent(inout) :: U(U_l1:U_h1,U_l2:U_h2,NVAR)

  ! Local variables
  integer          :: i,j,n
  integer          :: int_dom_spec
  logical          :: any_negative
  double precision :: dom_spec,x,rhoInv, sumrY, fac

  double precision, parameter :: eps = -1.d-16

  do j = lo(2),hi(2)
  do i = lo(1),hi(1)

     any_negative = .false.

     rhoInv = 1.d0/U(i,j,URHO)

     sumrY = 0.d0

     ! First deal with tiny undershoots by just setting them to zero
     do n = UFS, UFS+nspec-1
        if (U(i,j,n) .lt. 0.d0) then
           x = U(i,j,n) * rhoInv
           if (x .gt. eps) then
              U(i,j,n) = 0.d0
           else
              any_negative = .true.
           end if
        end if

        sumrY = sumrY + U(i,j,n)
     end do

     fac = U(i,j,URHO)/sumrY
     do n = UFS, UFS+nspec-1
        U(i,j,n) = U(i,j,n)*fac
     end do

     ! We know there are one or more undershoots needing correction 
     if (any_negative) then

        ! Find the dominant species
        dom_spec = 0.d0
        int_dom_spec = 0
        do n = UFS,UFS+nspec-1
           if (U(i,j,n) .gt. dom_spec) then
              dom_spec = U(i,j,n)
              int_dom_spec = n
           end if
        end do

        ! Now take care of undershoots greater in magnitude than 1e-16.
        do n = UFS, UFS+nspec-1
           
           if (U(i,j,n) .lt. 0.d0) then
              
              x = U(i,j,n)/U(i,j,URHO)
              
              ! Here we only print the bigger negative values
              if (x .lt. -1.d-2) then
                 print *,'Correcting negative species   ',n-UFS+1
                 print *,'   at cell (i)                ',i
                 print *,'Negative (rho*Y) is           ',U(i,j,n)
                 print *,'Negative      Y  is           ',x
                 print *,'Filling from dominant species ',int_dom_spec-UFS+1
                 print *,'  which had Y =               ',&
                      U(i,j,int_dom_spec) / U(i,j,URHO)
              end if

              ! Take enough from the dominant species to fill the negative one.
              U(i,j,int_dom_spec) = U(i,j,int_dom_spec) + U(i,j,n)
   
              ! Test that we didn't make the dominant species negative
              if (U(i,j,int_dom_spec) .lt. 0.d0) then 
                 print *,' Just made dominant species negative ',int_dom_spec-UFS+1,' at ',i
                 print *,'We were fixing species ',n-UFS+1,' which had value ',x
                 print *,'Dominant species became ',U(i,j,int_dom_spec) / U(i,j,URHO)
                 call bl_error("Error:: CNSReact_2d.f90 :: ca_enforce_nonnegative_species")
              end if

              ! Now set the negative species to zero
              U(i,j,n) = 0.d0

           end if

        end do

     end if
     
  end do
  end do

end subroutine rns_enforce_consistent_Y



! :: ----------------------------------------------------------
! :: Volume-weight average the fine grid data onto the coarse
! :: grid.  Overlap is given in coarse grid coordinates.
! ::
! :: INPUTS / OUTPUTS:
! ::  crse      <=  coarse grid data
! ::  clo,chi    => index limits of crse array interior
! ::  ngc        => number of ghost cells in coarse array
! ::  nvar	 => number of components in arrays
! ::  fine       => fine grid data
! ::  flo,fhi    => index limits of fine array interior
! ::  ngf        => number of ghost cells in fine array
! ::  rfine      => (ignore) used in 2-D RZ calc
! ::  lo,hi      => index limits of overlap (crse grid)
! ::  lrat       => refinement ratio
! ::
! :: NOTE:
! ::  Assumes all data cell centered
! :: ----------------------------------------------------------
! ::
      subroutine rns_avgdown(crse,c_l1,c_l2,c_h1,c_h2,nvar, &
                            cv,cv_l1,cv_l2,cv_h1,cv_h2, &
                            fine,f_l1,f_l2,f_h1,f_h2, &
                            fv,fv_l1,fv_l2,fv_h1,fv_h2,lo,hi,lrat)
      implicit none
      integer c_l1,c_l2,c_h1,c_h2
      integer cv_l1,cv_l2,cv_h1,cv_h2
      integer f_l1,f_l2,f_h1,f_h2
      integer fv_l1,fv_l2,fv_h1,fv_h2
      integer lo(2), hi(2)
      integer nvar, lrat(2)
      double precision crse(c_l1:c_h1,c_l2:c_h2,nvar)
      double precision cv(cv_l1:cv_h1,cv_l2:cv_h2)
      double precision fine(f_l1:f_h1,f_l2:f_h2,nvar)
      double precision fv(fv_l1:fv_h1,fv_l2:fv_h2)

      integer i, j, n, ic, jc, ioff, joff
      integer lenx, leny, mxlen
      integer lratx, lraty

      lratx = lrat(1)
      lraty = lrat(2)
      lenx = hi(1)-lo(1)+1
      leny = hi(2)-lo(2)+1
      mxlen = max(lenx,leny)

      if (lenx .eq. mxlen) then
         do n = 1, nvar
 
!           Set coarse grid to zero on overlap
            do jc = lo(2), hi(2)
               do ic = lo(1), hi(1)
                  crse(ic,jc,n) = 0.d0
               enddo
            enddo

!           Sum fine data
            do joff = 0, lraty-1
               do jc = lo(2), hi(2)
                  j = jc*lraty + joff
                  do ioff = 0, lratx-1
                     do ic = lo(1), hi(1)
                        i = ic*lratx + ioff
                        crse(ic,jc,n) = crse(ic,jc,n) + fv(i,j) * fine(i,j,n)
                     enddo
                  enddo
               enddo
            enddo

!           Divide out by volume weight
            do jc = lo(2), hi(2)
               do ic = lo(1), hi(1)
                  crse(ic,jc,n) = crse(ic,jc,n) / cv(ic,jc)
               enddo
            enddo
            
         enddo

      else

         do n = 1, nvar

!           Set coarse grid to zero on overlap
            do ic = lo(1), hi(1)
               do jc = lo(2), hi(2)
                  crse(ic,jc,n) = 0.d0
               enddo
            enddo
 
!           Sum fine data
            do ioff = 0, lratx-1
               do ic = lo(1), hi(1)
                  i = ic*lratx + ioff
                  do joff = 0, lraty-1
                     do jc = lo(2), hi(2)
                        j = jc*lraty + joff
                        crse(ic,jc,n) = crse(ic,jc,n) + fv(i,j) * fine(i,j,n)
                     enddo
                  enddo
               enddo
            enddo
             
!           Divide out by volume weight
            do ic = lo(1), hi(1)
               do jc = lo(2), hi(2)
                  crse(ic,jc,n) = crse(ic,jc,n) / cv(ic,jc)
               enddo
            enddo
            
         enddo

      end if

      end subroutine rns_avgdown

! :::
! ::: ------------------------------------------------------------------
! :::

      subroutine rns_estdt(u,u_l1,u_l2,u_h1,u_h2,lo,hi,dx,dt)
        use eos_module, only : eos_get_c
        use meth_params_module, only : NVAR, URHO, UMX, UMY, UEDEN, UTEMP, UFS, NSPEC
        implicit none

        integer u_l1,u_l2,u_h1,u_h2
        integer lo(2), hi(2)
        double precision u(u_l1:u_h1,u_l2:u_h2,NVAR)
        double precision dx(2), dt

        integer :: i, j
        double precision :: rhoInv, vx, vy, T, e, c, Y(NSPEC)

        do j = lo(2), hi(2)
        do i = lo(1), hi(1)
           rhoInv = 1.d0/u(i,j,URHO)

           vx = u(i,j,UMX)*rhoInv
           vy = u(i,j,UMY)*rhoInv
           T  = u(i,j,UTEMP)
           
           e = u(i,j,UEDEN)*rhoInv - 0.5d0*(vx**2+vy**2)
           
           if (NSPEC > 0) then
              Y = u(i,j,UFS:UFS+NSPEC-1)*rhoInv
           end if
           
           call eos_get_c(c,u(i,j,URHO),T,Y)

           dt = min(dt, dx(1)/(abs(vx)+c+1.d-50), dx(2)/(abs(vy)+c+1.d-50))
        end do
        end do

      end subroutine rns_estdt
