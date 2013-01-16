module trace_ppm_module

  implicit none

  private

  public tracexy_ppm, tracez_ppm

contains

  subroutine tracexy_ppm(q,c,flatn,qd_l1,qd_l2,qd_l3,qd_h1,qd_h2,qd_h3, &
                         Ip,Im, &
                         qxm,qxp,qym,qyp,qpd_l1,qpd_l2,qpd_l3,qpd_h1,qpd_h2,qpd_h3, &
                         ilo1,ilo2,ihi1,ihi2,dx,dy,dt,kc,k3d)

    use chemistry_module, only : nspec=>nspecies
    use meth_params_module, only : iorder, QVAR, QRHO, QU, QV, QW, &
         QREINT, QPRES, QFA, QFS, nadv, small_dens, &
         ppm_type

    implicit none

    integer,intent(in):: qd_l1,qd_l2,qd_l3,qd_h1,qd_h2,qd_h3
    integer,intent(in):: qpd_l1,qpd_l2,qpd_l3,qpd_h1,qpd_h2,qpd_h3
    integer,intent(in):: ilo1,ilo2,ihi1,ihi2
    integer,intent(in):: kc,k3d
    double precision,intent(in):: dx, dy, dt

    double precision,intent(in)::     q(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3,QVAR)
    double precision,intent(in)::     c(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3)
    double precision,intent(in):: flatn(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3)

    double precision,intent(in):: Ip(ilo1-1:ihi1+1,ilo2-1:ihi2+1,1:2,1:3,1:3,QVAR)
    double precision,intent(in):: Im(ilo1-1:ihi1+1,ilo2-1:ihi2+1,1:2,1:3,1:3,QVAR)

    double precision,intent(out):: qxm(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)
    double precision,intent(out):: qxp(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)
    double precision,intent(out):: qym(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)
    double precision,intent(out):: qyp(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)

    ! Local variables
    integer i, j
    integer n, iadv
    integer ns, ispec

    double precision dtdx, dtdy
    double precision cc, csq, rho, u, v, w, p, rhoe
    double precision drho, du, dv, dw, dp, drhoe
    double precision dup, dvp, dwp, dpp
    double precision dum, dvm, dwm, dpm

    double precision enth, alpham, alphap, alpha0r, alpha0e
    double precision alpha0u, alpha0v, alpha0w
    double precision apright, amright, azrright, azeright
    double precision azu1rght, azv1rght, azw1rght
    double precision apleft, amleft, azrleft, azeleft
    double precision azu1left, azv1left, azw1left

    if (ppm_type .eq. 0) then
       print *,'Oops -- shouldnt be in tracexy_ppm with ppm_type = 0'
       call bl_error("Error:: trace_ppm_3d.f90 :: tracexy_ppm")
    end if

    dtdx = dt/dx
    dtdy = dt/dy

    !!!!!!!!!!!!!!!
    ! PPM CODE
    !!!!!!!!!!!!!!!

    ! Trace to left and right edges using upwind PPM
    !$OMP PARALLEL DO PRIVATE(i,j,cc,csq,rho,u,v,w,p,rhoe,enth,dum,dvm,dwm,dpm) &
    !$OMP PRIVATE(drho,du,dv,dw,dp,drhoe,dup,dvp,dwp,dpp,alpham,alphap,alpha0r) &
    !$OMP PRIVATE(alpha0e,alpha0v,alpha0w,amright,apright,azrright,azeright,azv1rght,azw1rght) &
    !$OMP PRIVATE(amleft,apleft,azrleft,azeleft,azv1left,azw1left)
    do j = ilo2-1, ihi2+1
       do i = ilo1-1, ihi1+1

          cc = c(i,j,k3d)
          csq = cc**2
          rho = q(i,j,k3d,QRHO)
          u = q(i,j,k3d,QU)
          v = q(i,j,k3d,QV)
          w = q(i,j,k3d,QW)
          p = q(i,j,k3d,QPRES)
          rhoe = q(i,j,k3d,QREINT)
          enth = ( (rhoe+p)/rho )/csq

          ! plus state on face i
          dum    = flatn(i,j,k3d)*(u    - Im(i,j,kc,1,1,QU))
         ! dvm    = flatn(i,j,k3d)*(v    - Im(i,j,kc,1,1,QV))
         ! dwm    = flatn(i,j,k3d)*(w    - Im(i,j,kc,1,1,QW))
          dpm    = flatn(i,j,k3d)*(p    - Im(i,j,kc,1,1,QPRES))

          drho  = flatn(i,j,k3d)*(rho  - Im(i,j,kc,1,2,QRHO))
         ! du    = flatn(i,j,k3d)*(u    - Im(i,j,kc,1,2,QU))
          dv    = flatn(i,j,k3d)*(v    - Im(i,j,kc,1,2,QV))
          dw    = flatn(i,j,k3d)*(w    - Im(i,j,kc,1,2,QW))
          dp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,1,2,QPRES))
          drhoe = flatn(i,j,k3d)*(rhoe - Im(i,j,kc,1,2,QREINT))

          dup    = flatn(i,j,k3d)*(u    - Im(i,j,kc,1,3,QU))
         ! dvp    = flatn(i,j,k3d)*(v    - Im(i,j,kc,1,3,QV))
         ! dwp    = flatn(i,j,k3d)*(w    - Im(i,j,kc,1,3,QW))
          dpp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,1,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dum)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dup)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0v = dv
          alpha0w = dw

          if (u-cc .gt. 0.d0) then
             amright = 0.d0
          else if (u-cc .lt. 0.d0) then
             amright = -alpham
          else
             amright = -0.5d0*alpham
          endif
          if (u+cc .gt. 0.d0) then
             apright = 0.d0
          else if (u+cc .lt. 0.d0) then
             apright = -alphap
          else
             apright = -0.5d0*alphap
          endif
          if (u .gt. 0.d0) then
             azrright = 0.d0
             azeright = 0.d0
             azv1rght = 0.d0
             azw1rght = 0.d0
          else if (u .lt. 0.d0) then
             azrright = -alpha0r
             azeright = -alpha0e
             azv1rght = -alpha0v
             azw1rght = -alpha0w
          else
             azrright = -0.5d0*alpha0r
             azeright = -0.5d0*alpha0e
             azv1rght = -0.5d0*alpha0v
             azw1rght = -0.5d0*alpha0w
          endif

          if (i .ge. ilo1) then
             qxp(i,j,kc,QRHO) = rho + apright + amright + azrright
             qxp(i,j,kc,QRHO) = max(small_dens,qxp(i,j,kc,QRHO))
             qxp(i,j,kc,QU) = u + (apright - amright)*cc/rho
             qxp(i,j,kc,QV) = v + azv1rght
             qxp(i,j,kc,QW) = w + azw1rght
             qxp(i,j,kc,QPRES) = p + (apright + amright)*csq
             qxp(i,j,kc,QREINT) = rhoe + (apright + amright)*enth*csq + azeright
          end if

          ! minus state on face i+1
          dum    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,1,1,QU))
        !  dvm    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,1,1,QV))
        !  dwm    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,1,1,QW))
          dpm    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,1,1,QPRES))

          drho  = flatn(i,j,k3d)*(rho  - Ip(i,j,kc,1,2,QRHO))
        !  du    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,1,2,QU))
          dv    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,1,2,QV))
          dw    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,1,2,QW))
          dp    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,1,2,QPRES))
          drhoe = flatn(i,j,k3d)*(rhoe - Ip(i,j,kc,1,2,QREINT))

          dup    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,1,3,QU))
        !  dvp    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,1,3,QV))
        !  dwp    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,1,3,QW))
          dpp    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,1,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dum)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dup)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0v = dv
          alpha0w = dw

          if (u-cc .gt. 0.d0) then
             amleft = -alpham
          else if (u-cc .lt. 0.d0) then
             amleft = 0.d0
          else
             amleft = -0.5d0*alpham
          endif
          if (u+cc .gt. 0.d0) then
             apleft = -alphap
          else if (u+cc .lt. 0.d0) then
             apleft = 0.d0
          else
             apleft = -0.5d0*alphap
          endif
          if (u .gt. 0.d0) then
             azrleft = -alpha0r
             azeleft = -alpha0e
             azv1left = -alpha0v
             azw1left = -alpha0w
          else if (u .lt. 0.d0) then
             azrleft = 0.d0
             azeleft = 0.d0
             azv1left = 0.d0
             azw1left = 0.d0
          else
             azrleft = -0.5d0*alpha0r
             azeleft = -0.5d0*alpha0e
             azv1left = -0.5d0*alpha0v
             azw1left = -0.5d0*alpha0w
          endif

          if (i .le. ihi1) then
             qxm(i+1,j,kc,QRHO) = rho + apleft + amleft + azrleft
             qxm(i+1,j,kc,QRHO) = max(qxm(i+1,j,kc,QRHO),small_dens)
             qxm(i+1,j,kc,QU) = u + (apleft - amleft)*cc/rho
             qxm(i+1,j,kc,QV) = v + azv1left
             qxm(i+1,j,kc,QW) = w + azw1left
             qxm(i+1,j,kc,QPRES) = p + (apleft + amleft)*csq
             qxm(i+1,j,kc,QREINT) = rhoe + (apleft + amleft)*enth*csq + azeleft
          end if

       end do
    end do
    !$OMP END PARALLEL DO

    ! Now do the passively advected quantities
    !$OMP PARALLEL DO PRIVATE(iadv,n,i,j,u) IF(nadv.gt.1)
    do iadv = 1, nadv
       n = QFA + iadv - 1
       do j = ilo2-1, ihi2+1

          ! plus state on face i
          do i = ilo1, ihi1+1
             u = q(i,j,k3d,QU)
             if (u .gt. 0.d0) then
                qxp(i,j,kc,n) = q(i,j,k3d,n)
             else if (u .lt. 0.d0) then
                qxp(i,j,kc,n) = q(i,j,k3d,n) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,1,2,n) - q(i,j,k3d,n))
             else
                qxp(i,j,kc,n) = q(i,j,k3d,n) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,1,2,n) - q(i,j,k3d,n))
             endif
          enddo

          ! minus state on face i+1
          do i = ilo1-1, ihi1
             u = q(i,j,k3d,QU)
             if (u .gt. 0.d0) then
                qxm(i+1,j,kc,n) = q(i,j,k3d,n) &
                     + flatn(i,j,k3d)*(Ip(i,j,kc,1,2,n) - q(i,j,k3d,n))
             else if (u .lt. 0.d0) then
                qxm(i+1,j,kc,n) = q(i,j,k3d,n)
             else
                qxm(i+1,j,kc,n) = q(i,j,k3d,n) &
                     + 0.5d0*flatn(i,j,k3d)*(Ip(i,j,kc,1,2,n) - q(i,j,k3d,n))
             endif
          enddo

       enddo
    enddo
    !$OMP END PARALLEL DO

    !$OMP PARALLEL DO PRIVATE(ispec,ns,i,j,u) IF(Nspec.gt.1)
    do ispec = 1, Nspec
       ns = QFS + ispec - 1

       do j = ilo2-1, ihi2+1

          ! plus state on face i
          do i = ilo1, ihi1+1
             u = q(i,j,k3d,QU)
             if (u .gt. 0.d0) then
                qxp(i,j,kc,ns) = q(i,j,k3d,ns)
             else if (u .lt. 0.d0) then
                qxp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,1,2,ns) - q(i,j,k3d,ns))
             else
                qxp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,1,2,ns) - q(i,j,k3d,ns))
             endif
          enddo

          ! minus state on face i+1
          do i = ilo1-1, ihi1
             u = q(i,j,k3d,QU)
             if (u .gt. 0.d0) then
                qxm(i+1,j,kc,ns) = q(i,j,k3d,ns) &
                     + flatn(i,j,k3d)*(Ip(i,j,kc,1,2,ns) - q(i,j,k3d,ns))
             else if (u .lt. 0.d0) then
                qxm(i+1,j,kc,ns) = q(i,j,k3d,ns)
             else
                qxm(i+1,j,kc,ns) = q(i,j,k3d,ns) &
                     + 0.5d0*flatn(i,j,k3d)*(Ip(i,j,kc,1,2,ns) - q(i,j,k3d,ns))
             endif
          enddo

       enddo
    enddo
    !$OMP END PARALLEL DO

    ! Trace to bottom and top edges using upwind PPM
    !$OMP PARALLEL DO PRIVATE(i,j,cc,csq,rho,u,v,w,p,rhoe,enth,dum,dvm,dwm,dpm) &
    !$OMP PRIVATE(drho,du,dv,dw,dp,drhoe,dup,dvp,dwp,dpp,alpham,alphap,alpha0r) &
    !$OMP PRIVATE(alpha0e,alpha0u,alpha0w,amright,apright,azrright,azeright,azu1rght,azw1rght,amleft) &
    !$OMP PRIVATE(apleft,azrleft,azeleft,azu1left,azw1left)
    do j = ilo2-1, ihi2+1
       do i = ilo1-1, ihi1+1

          cc = c(i,j,k3d)
          csq = cc**2
          rho = q(i,j,k3d,QRHO)
          u = q(i,j,k3d,QU)
          v = q(i,j,k3d,QV)
          w = q(i,j,k3d,QW)
          p = q(i,j,k3d,QPRES)
          rhoe = q(i,j,k3d,QREINT)
          enth = ( (rhoe+p)/rho )/csq

          ! plus state on face j
         ! dum    = flatn(i,j,k3d)*(u    - Im(i,j,kc,2,1,QU))
          dvm    = flatn(i,j,k3d)*(v    - Im(i,j,kc,2,1,QV))
         ! dwm    = flatn(i,j,k3d)*(w    - Im(i,j,kc,2,1,QW))
          dpm    = flatn(i,j,k3d)*(p    - Im(i,j,kc,2,1,QPRES))

          drho  = flatn(i,j,k3d)*(rho  - Im(i,j,kc,2,2,QRHO))
          du    = flatn(i,j,k3d)*(u    - Im(i,j,kc,2,2,QU))
         ! dv    = flatn(i,j,k3d)*(v    - Im(i,j,kc,2,2,QV))
          dw    = flatn(i,j,k3d)*(w    - Im(i,j,kc,2,2,QW))
          dp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,2,2,QPRES))
          drhoe = flatn(i,j,k3d)*(rhoe - Im(i,j,kc,2,2,QREINT))

         ! dup    = flatn(i,j,k3d)*(u    - Im(i,j,kc,2,3,QU))
          dvp    = flatn(i,j,k3d)*(v    - Im(i,j,kc,2,3,QV))
         ! dwp    = flatn(i,j,k3d)*(w    - Im(i,j,kc,2,3,QW))
          dpp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,2,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dvm)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dvp)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0u = du
          alpha0w = dw

          if (v-cc .gt. 0.d0) then
             amright = 0.d0
          else if (v-cc .lt. 0.d0) then
             amright = -alpham
          else
             amright = -0.5d0*alpham
          endif
          if (v+cc .gt. 0.d0) then
             apright = 0.d0
          else if (v+cc .lt. 0.d0) then
             apright = -alphap
          else
             apright = -0.5d0*alphap
          endif
          if (v .gt. 0.d0) then
             azrright = 0.d0
             azeright = 0.d0
             azu1rght = 0.d0
             azw1rght = 0.d0
          else if (v .lt. 0.d0) then
             azrright = -alpha0r
             azeright = -alpha0e
             azu1rght = -alpha0u
             azw1rght = -alpha0w
          else
             azrright = -0.5d0*alpha0r
             azeright = -0.5d0*alpha0e
             azu1rght = -0.5d0*alpha0u
             azw1rght = -0.5d0*alpha0w
          endif

          if (j .ge. ilo2) then
             qyp(i,j,kc,QRHO) = rho + apright + amright + azrright
             qyp(i,j,kc,QRHO) = max(small_dens, qyp(i,j,kc,QRHO))
             qyp(i,j,kc,QV) = v + (apright - amright)*cc/rho
             qyp(i,j,kc,QU) = u + azu1rght
             qyp(i,j,kc,QW) = w + azw1rght
             qyp(i,j,kc,QPRES) = p + (apright + amright)*csq
             qyp(i,j,kc,QREINT) = rhoe + (apright + amright)*enth*csq + azeright
          end if

          ! minus state on face j+1
         ! dum    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,2,1,QU))
          dvm    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,2,1,QV))
         ! dwm    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,2,1,QW))
          dpm    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,2,1,QPRES))

          drho  = flatn(i,j,k3d)*(rho  - Ip(i,j,kc,2,2,QRHO))
          du    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,2,2,QU))
         ! dv    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,2,2,QV))
          dw    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,2,2,QW))
          dp    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,2,2,QPRES))
          drhoe = flatn(i,j,k3d)*(rhoe - Ip(i,j,kc,2,2,QREINT))

         ! dup    = flatn(i,j,k3d)*(u    - Ip(i,j,kc,2,3,QU))
          dvp    = flatn(i,j,k3d)*(v    - Ip(i,j,kc,2,3,QV))
         ! dwp    = flatn(i,j,k3d)*(w    - Ip(i,j,kc,2,3,QW))
          dpp    = flatn(i,j,k3d)*(p    - Ip(i,j,kc,2,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dvm)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dvp)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0u = du
          alpha0w = dw

          if (v-cc .gt. 0.d0) then
             amleft = -alpham
          else if (v-cc .lt. 0.d0) then
             amleft = 0.d0
          else
             amleft = -0.5d0*alpham
          endif
          if (v+cc .gt. 0.d0) then
             apleft = -alphap
          else if (v+cc .lt. 0.d0) then
             apleft = 0.d0
          else
             apleft = -0.5d0*alphap
          endif
          if (v .gt. 0.d0) then
             azrleft = -alpha0r
             azeleft = -alpha0e
             azu1left = -alpha0u
             azw1left = -alpha0w
          else if (v .lt. 0.d0) then
             azrleft = 0.d0
             azeleft = 0.d0
             azu1left = 0.d0
             azw1left = 0.d0
          else
             azrleft = -0.5d0*alpha0r
             azeleft = -0.5d0*alpha0e
             azu1left = -0.5d0*alpha0u
             azw1left = -0.5d0*alpha0w
          endif

          if (j .le. ihi2) then
             qym(i,j+1,kc,QRHO) = rho + apleft + amleft + azrleft
             qym(i,j+1,kc,QRHO) = max(small_dens, qym(i,j+1,kc,QRHO))
             qym(i,j+1,kc,QV) = v + (apleft - amleft)*cc/rho
             qym(i,j+1,kc,QU) = u + azu1left
             qym(i,j+1,kc,QW) = w + azw1left
             qym(i,j+1,kc,QPRES) = p + (apleft + amleft)*csq
             qym(i,j+1,kc,QREINT) = rhoe + (apleft + amleft)*enth*csq + azeleft
          end if

       end do
    end do
    !$OMP END PARALLEL DO

    ! Now do the passively advected quantities
    !$OMP PARALLEL DO PRIVATE(iadv,n,i,j,v) IF(nadv.gt.1)
    do iadv = 1, nadv
       n = QFA + iadv - 1
       do i = ilo1-1, ihi1+1

          ! plus state on face j
          do j = ilo2, ihi2+1
             v = q(i,j,k3d,QV)
             if (v .gt. 0.d0) then
                qyp(i,j,kc,n) = q(i,j,k3d,n)
             else if (v .lt. 0.d0) then
                qyp(i,j,kc,n) = q(i,j,k3d,n) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,2,2,n) - q(i,j,k3d,n))
             else
                qyp(i,j,kc,n) = q(i,j,k3d,n) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,2,2,n) - q(i,j,k3d,n))
             endif
          enddo

          ! minus state on face j+1
          do j = ilo2-1, ihi2
             v = q(i,j,k3d,QV)
             if (v .gt. 0.d0) then
                qym(i,j+1,kc,n) = q(i,j,k3d,n) &
                     + flatn(i,j,k3d)*(Ip(i,j,kc,2,2,n) - q(i,j,k3d,n))
             else if (v .lt. 0.d0) then
                qym(i,j+1,kc,n) = q(i,j,k3d,n)
             else
                qym(i,j+1,kc,n) = q(i,j,k3d,n) &
                     + 0.5d0*flatn(i,j,k3d)*(Ip(i,j,kc,2,2,n) - q(i,j,k3d,n))
             endif
          enddo

       enddo
    enddo
    !$OMP END PARALLEL DO

    !$OMP PARALLEL DO PRIVATE(ispec,ns,i,j,v) IF(Nspec.gt.1)
    do ispec = 1, Nspec
       ns = QFS + ispec - 1
       do i = ilo1-1, ihi1+1

          ! plus state on face j
          do j = ilo2, ihi2+1
             v = q(i,j,k3d,QV)
             if (v .gt. 0.d0) then
                qyp(i,j,kc,ns) = q(i,j,k3d,ns)
             else if (v .lt. 0.d0) then
                qyp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,2,2,ns) - q(i,j,k3d,ns))
             else
                qyp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,2,2,ns) - q(i,j,k3d,ns))
             endif
          enddo

          ! minus state on face j+1
          do j = ilo2-1, ihi2
             v = q(i,j,k3d,QV)
             if (v .gt. 0.d0) then
                qym(i,j+1,kc,ns) = q(i,j,k3d,ns) &
                     + flatn(i,j,k3d)*(Ip(i,j,kc,2,2,ns) - q(i,j,k3d,ns))
             else if (v .lt. 0.d0) then
                qym(i,j+1,kc,ns) = q(i,j,k3d,ns)
             else
                qym(i,j+1,kc,ns) = q(i,j,k3d,ns) &
                     + 0.5d0*flatn(i,j,k3d)*(Ip(i,j,kc,2,2,ns) - q(i,j,k3d,ns))
             endif
          enddo

       enddo
    enddo
    !$OMP END PARALLEL DO

  end subroutine tracexy_ppm

  ! ::: 
  ! ::: ------------------------------------------------------------------
  ! ::: 

  subroutine tracez_ppm(q,c,flatn,qd_l1,qd_l2,qd_l3,qd_h1,qd_h2,qd_h3, &
                        Ip,Im, &
                        qzm,qzp,qpd_l1,qpd_l2,qpd_l3,qpd_h1,qpd_h2,qpd_h3, &
                        ilo1,ilo2,ihi1,ihi2,dz,dt,km,kc,k3d)

    use chemistry_module, only : Nspec=>nspecies
    use meth_params_module, only : iorder, QVAR, QRHO, QU, QV, QW, &
         QREINT, QPRES, QFA, QFS, nadv, small_dens, &
         ppm_type

    implicit none

    integer,intent(in):: qd_l1,qd_l2,qd_l3,qd_h1,qd_h2,qd_h3
    integer,intent(in):: qpd_l1,qpd_l2,qpd_l3,qpd_h1,qpd_h2,qpd_h3
    integer,intent(in):: ilo1,ilo2,ihi1,ihi2
    integer,intent(in):: km,kc,k3d
    double precision,intent(in):: dz, dt

    double precision,intent(in)::     q(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3,QVAR)
    double precision,intent(in)::     c(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3)
    double precision,intent(in):: flatn(qd_l1:qd_h1,qd_l2:qd_h2,qd_l3:qd_h3)

    double precision,intent(in)::   Ip(ilo1-1:ihi1+1,ilo2-1:ihi2+1,1:2,1:3,1:3,QVAR)
    double precision,intent(in)::   Im(ilo1-1:ihi1+1,ilo2-1:ihi2+1,1:2,1:3,1:3,QVAR)

    double precision,intent(out):: qzm(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)
    double precision,intent(out):: qzp(qpd_l1:qpd_h1,qpd_l2:qpd_h2,qpd_l3:qpd_h3,QVAR)

    !     Local variables
    integer i, j
    integer n, iadv
    integer ns, ispec

    double precision dtdz
    double precision cc, csq, rho, u, v, w, p, rhoe
    double precision dup, dvp, dwp, dpp
    double precision dum, dvm, dwm, dpm

    double precision drho, du, dv, dw, dp, drhoe
    double precision enth, alpham, alphap, alpha0r, alpha0e
    double precision alpha0u, alpha0v
    double precision apright, amright, azrright, azeright
    double precision azu1rght, azv1rght
    double precision apleft, amleft, azrleft, azeleft
    double precision azu1left, azv1left

    if (ppm_type .eq. 0) then
       print *,'Oops -- shouldnt be in tracez_ppm with ppm_type = 0'
       call bl_error("Error:: ppm_3d.f90 :: tracez_ppm")
    end if

    dtdz = dt/dz

    !!!!!!!!!!!!!!!
    ! PPM CODE
    !!!!!!!!!!!!!!!

    ! Trace to left and right edges using upwind PPM
    !$OMP PARALLEL DO PRIVATE(i,j,cc,csq,rho,u,v,w,p,rhoe,enth,dum,dvm,dwm,dpm) &
    !$OMP PRIVATE(drho,du,dv,dw,dp,drhoe,dup,dvp,dwp,dpp,alpham,alphap,alpha0r,alpha0e) &
    !$OMP PRIVATE(alpha0u,alpha0v,amright,apright,azrright,azeright,azu1rght,azv1rght,amleft,apleft)&
    !$OMP PRIVATE(azrleft,azeleft,azu1left,azv1left)
    do j = ilo2-1, ihi2+1
       do i = ilo1-1, ihi1+1

          cc = c(i,j,k3d)
          csq = cc**2
          rho = q(i,j,k3d,QRHO)
          u = q(i,j,k3d,QU)
          v = q(i,j,k3d,QV)
          w = q(i,j,k3d,QW)
          p = q(i,j,k3d,QPRES)
          rhoe = q(i,j,k3d,QREINT)
          enth = ( (rhoe+p)/rho )/csq

          ! plus state on face kc
         ! dum    = flatn(i,j,k3d)*(u    - Im(i,j,kc,3,1,QU))
         ! dvm    = flatn(i,j,k3d)*(v    - Im(i,j,kc,3,1,QV))
          dwm    = flatn(i,j,k3d)*(w    - Im(i,j,kc,3,1,QW))
          dpm    = flatn(i,j,k3d)*(p    - Im(i,j,kc,3,1,QPRES))

          drho  = flatn(i,j,k3d)*(rho  - Im(i,j,kc,3,2,QRHO))
          du    = flatn(i,j,k3d)*(u    - Im(i,j,kc,3,2,QU))
          dv    = flatn(i,j,k3d)*(v    - Im(i,j,kc,3,2,QV))
         ! dw    = flatn(i,j,k3d)*(w    - Im(i,j,kc,3,2,QW))
          dp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,3,2,QPRES))
          drhoe = flatn(i,j,k3d)*(rhoe - Im(i,j,kc,3,2,QREINT))

         ! dup    = flatn(i,j,k3d)*(u    - Im(i,j,kc,3,3,QU))
         ! dvp    = flatn(i,j,k3d)*(v    - Im(i,j,kc,3,3,QV))
          dwp    = flatn(i,j,k3d)*(w    - Im(i,j,kc,3,3,QW))
          dpp    = flatn(i,j,k3d)*(p    - Im(i,j,kc,3,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dwm)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dwp)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0u = du
          alpha0v = dv

          if (w-cc .gt. 0.d0) then
             amright = 0.d0
          else if (w-cc .lt. 0.d0) then
             amright = -alpham
          else
             amright = -0.5d0*alpham
          endif
          if (w+cc .gt. 0.d0) then
             apright = 0.d0
          else if (w+cc .lt. 0.d0) then
             apright = -alphap
          else
             apright = -0.5d0*alphap
          endif
          if (w .gt. 0.d0) then
             azrright = 0.d0
             azeright = 0.d0
             azu1rght = 0.d0
             azv1rght = 0.d0
          else if (w .lt. 0.d0) then
             azrright = -alpha0r
             azeright = -alpha0e
             azu1rght = -alpha0u
             azv1rght = -alpha0v
          else
             azrright = -0.5d0*alpha0r
             azeright = -0.5d0*alpha0e
             azu1rght = -0.5d0*alpha0u
             azv1rght = -0.5d0*alpha0v
          endif

          qzp(i,j,kc,QRHO) = rho + apright + amright + azrright
          qzp(i,j,kc,QRHO) = max(small_dens, qzp(i,j,kc,QRHO))
          qzp(i,j,kc,QW) = w + (apright - amright)*cc/rho
          qzp(i,j,kc,QU) = u + azu1rght
          qzp(i,j,kc,QV) = v + azv1rght
          qzp(i,j,kc,QPRES) = p + (apright + amright)*csq
          qzp(i,j,kc,QREINT) = rhoe + (apright + amright)*enth*csq + azeright

          ! minus state on face kc
          ! note this is different from how we do 1D, 2D, and the
          ! x and y-faces in 3D, where the analogous thing would have
          ! been to find the minus state on face kc+1
          cc = c(i,j,k3d-1)
          csq = cc**2
          rho = q(i,j,k3d-1,QRHO)
          u = q(i,j,k3d-1,QU)
          v = q(i,j,k3d-1,QV)
          w = q(i,j,k3d-1,QW)
          p = q(i,j,k3d-1,QPRES)
          rhoe = q(i,j,k3d-1,QREINT)
          enth = ( (rhoe+p)/rho )/csq

         ! dum    = flatn(i,j,k3d-1)*(u    - Ip(i,j,km,3,1,QU))
         ! dvm    = flatn(i,j,k3d-1)*(v    - Ip(i,j,km,3,1,QV))
          dwm    = flatn(i,j,k3d-1)*(w    - Ip(i,j,km,3,1,QW))
          dpm    = flatn(i,j,k3d-1)*(p    - Ip(i,j,km,3,1,QPRES))

          drho  = flatn(i,j,k3d-1)*(rho  - Ip(i,j,km,3,2,QRHO))
          du    = flatn(i,j,k3d-1)*(u    - Ip(i,j,km,3,2,QU))
          dv    = flatn(i,j,k3d-1)*(v    - Ip(i,j,km,3,2,QV))
         ! dw    = flatn(i,j,k3d-1)*(w    - Ip(i,j,km,3,2,QW))
          dp    = flatn(i,j,k3d-1)*(p    - Ip(i,j,km,3,2,QPRES))
          drhoe = flatn(i,j,k3d-1)*(rhoe - Ip(i,j,km,3,2,QREINT))

         ! dup    = flatn(i,j,k3d-1)*(u    - Ip(i,j,km,3,3,QU))
         ! dvp    = flatn(i,j,k3d-1)*(v    - Ip(i,j,km,3,3,QV))
          dwp    = flatn(i,j,k3d-1)*(w    - Ip(i,j,km,3,3,QW))
          dpp    = flatn(i,j,k3d-1)*(p    - Ip(i,j,km,3,3,QPRES))

          alpham = 0.5d0*(dpm/(rho*cc) - dwm)*rho/cc
          alphap = 0.5d0*(dpp/(rho*cc) + dwp)*rho/cc
          alpha0r = drho - dp/csq
          alpha0e = drhoe - dp*enth
          alpha0u = du
          alpha0v = dv

          if (w-cc .gt. 0.d0) then
             amleft = -alpham
          else if (w-cc .lt. 0.d0) then
             amleft = 0.d0
          else
             amleft = -0.5d0*alpham
          endif
          if (w+cc .gt. 0.d0) then
             apleft = -alphap
          else if (w+cc .lt. 0.d0) then
             apleft = 0.d0
          else
             apleft = -0.5d0*alphap
          endif
          if (w .gt. 0.d0) then
             azrleft = -alpha0r
             azeleft = -alpha0e
             azu1left = -alpha0u
             azv1left = -alpha0v
          else if (w .lt. 0.d0) then
             azrleft = 0.d0
             azeleft = 0.d0
             azu1left = 0.d0
             azv1left = 0.d0
          else
             azrleft = -0.5d0*alpha0r
             azeleft = -0.5d0*alpha0e
             azu1left = -0.5d0*alpha0u
             azv1left = -0.5d0*alpha0v
          endif

          qzm(i,j,kc,QRHO) = rho + apleft + amleft + azrleft
          qzm(i,j,kc,QRHO) = max(small_dens, qzm(i,j,kc,QRHO))
          qzm(i,j,kc,QW) = w + (apleft - amleft)*cc/rho
          qzm(i,j,kc,QU) = u + azu1left
          qzm(i,j,kc,QV) = v + azv1left
          qzm(i,j,kc,QPRES) = p + (apleft + amleft)*csq
          qzm(i,j,kc,QREINT) = rhoe + (apleft + amleft)*enth*csq + azeleft

       end do
    end do
    !$OMP END PARALLEL DO

    ! Now do the passively advected quantities
    !$OMP PARALLEL DO PRIVATE(iadv,n,i,j,w) IF(nadv.gt.1)
    do iadv = 1, nadv
       n = QFA + iadv - 1
       do j = ilo2-1, ihi2+1
          do i = ilo1-1, ihi1+1

             ! plus state on face kc
             w = q(i,j,k3d,QW)
             if (w .gt. 0.d0) then
                qzp(i,j,kc,n) = q(i,j,k3d,n)
             else if (w .lt. 0.d0) then
                qzp(i,j,kc,n) = q(i,j,k3d,n) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,3,2,n) - q(i,j,k3d,n))
             else
                qzp(i,j,kc,n) = q(i,j,k3d,n) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,3,2,n) - q(i,j,k3d,n))
             endif

             ! minus state on face k
             w = q(i,j,k3d-1,QW)
             if (w .gt. 0.d0) then
                qzm(i,j,kc,n) = q(i,j,k3d-1,n) &
                     + flatn(i,j,k3d-1)*(Ip(i,j,km,3,2,n) - q(i,j,k3d-1,n))
             else if (w .lt. 0.d0) then
                qzm(i,j,kc,n) = q(i,j,k3d-1,n)
             else
                qzm(i,j,kc,n) = q(i,j,k3d-1,n) &
                     + 0.5d0*flatn(i,j,k3d-1)*(Ip(i,j,km,3,2,n) - q(i,j,k3d-1,n))
             endif

          enddo
       enddo
    enddo
    !$OMP END PARALLEL DO

    !$OMP PARALLEL DO PRIVATE(ispec,ns,i,j,w) IF(Nspec.gt.1)
    do ispec = 1, Nspec
       ns = QFS + ispec - 1
       do j = ilo2-1, ihi2+1
          do i = ilo1-1, ihi1+1

             ! plus state on face kc
             w = q(i,j,k3d,QW)
             if (w .gt. 0.d0) then
                qzp(i,j,kc,ns) = q(i,j,k3d,ns)
             else if (w .lt. 0.d0) then
                qzp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + flatn(i,j,k3d)*(Im(i,j,kc,3,2,ns) - q(i,j,k3d,ns))
             else
                qzp(i,j,kc,ns) = q(i,j,k3d,ns) &
                     + 0.5d0*flatn(i,j,k3d)*(Im(i,j,kc,3,2,ns) - q(i,j,k3d,ns))
             endif

             ! minus state on face k
             w = q(i,j,k3d-1,QW)
             if (w .gt. 0.d0) then
                qzm(i,j,kc,ns) = q(i,j,k3d-1,ns) &
                     + flatn(i,j,k3d-1)*(Ip(i,j,km,3,2,ns) - q(i,j,k3d-1,ns))
             else if (w .lt. 0.d0) then
                qzm(i,j,kc,ns) = q(i,j,k3d-1,ns)
             else
                qzm(i,j,kc,ns) = q(i,j,k3d-1,ns) &
                     + 0.5d0*flatn(i,j,k3d-1)*(Ip(i,j,km,3,2,ns) - q(i,j,k3d-1,ns))
             endif

          enddo
       enddo
    enddo
    !$OMP END PARALLEL DO

  end subroutine tracez_ppm

end module trace_ppm_module
