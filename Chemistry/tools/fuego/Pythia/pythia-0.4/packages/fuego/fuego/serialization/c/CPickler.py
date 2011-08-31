#!/usr/bin/env python
# 
#  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 
#                               Michael A.G. Aivazis
#                        California Institute of Technology
#                        (C) 1998-2003 All Rights Reserved
# 
#  <LicenseText>
# 
#  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# 

from weaver.mills.CMill import CMill

from pyre.units.pressure import atm
from pyre.units.SI import meter, second, mole, kelvin
from pyre.units.length import cm
from pyre.units.energy import cal, kcal, J, kJ, erg
from pyre.handbook.constants.fundamental import avogadro
from pyre.handbook.constants.fundamental import gas_constant as R

smallnum = 1e-100
R = 8.31451e7 * erg/mole/kelvin
#Rc = 1.987215583 * cal/mole/kelvin
Rc = 1.98721558317399617591 * cal/mole/kelvin
Patm = 1013250.0
sym  = ""
fsym = "_"

class speciesDb:
    def __init__(self, id, name, mwt):
        self.id = id
        self.symbol = name
        self.weight = mwt
        return


class CPickler(CMill):


    def __init__(self):
        CMill.__init__(self)
        self.species = []
        self.nSpecies = 0
        return


    def _setSpecies(self, mechanism):
        """ For internal use """
        import pyre
        periodic = pyre.handbook.periodicTable()
        
        nSpecies = len(mechanism.species())
        self.species = [ 0.0 for x in range(nSpecies) ]
        
        for species in mechanism.species():
            weight = 0.0 
            for elem, coef in species.composition:
                aw = mechanism.element(elem).weight
                if not aw:
                    aw = periodic.symbol(elem.capitalize()).atomicWeight
                weight += coef * aw

            tempsp = speciesDb(species.id, species.symbol, weight)
            self.species[species.id] = tempsp

        self.nSpecies = nSpecies
        return


    def _renderDocument(self, mechanism, options=None):

        self._setSpecies(mechanism)
        self._includes()
        self._declarations()

        #self._main(mechanism)

        # chemkin wrappers
        self._ckindx(mechanism)
        self._ckinit(mechanism)
        self._ckxnum(mechanism)
        self._cksnum(mechanism)
        self._cksyme(mechanism)
        self._cksyms(mechanism)
        self._ckrp(mechanism)
        
        self._ckpx(mechanism)
        self._ckpy(mechanism)
        self._ckpc(mechanism)
        self._ckrhox(mechanism)
        self._ckrhoy(mechanism)
        self._ckrhoc(mechanism)
        self._ckwt(mechanism)
        self._ckmmwy(mechanism)
        self._ckmmwx(mechanism)
        self._ckmmwc(mechanism)
        self._ckytx(mechanism)
        self._ckytcp(mechanism)
        self._ckytcr(mechanism)
        self._ckxty(mechanism)
        self._ckxtcp(mechanism)
        self._ckxtcr(mechanism)
        self._ckctx(mechanism)
        self._ckcty(mechanism)
        
        self._ckcpor(mechanism)
        self._ckhort(mechanism)
        self._cksor(mechanism)
        
        self._ckcvml(mechanism)
        self._ckcpml(mechanism)
        self._ckuml(mechanism)
        self._ckhml(mechanism)
        self._ckgml(mechanism)
        self._ckaml(mechanism)
        self._cksml(mechanism)
        
        self._ckcvms(mechanism)
        self._ckcpms(mechanism)
        self._ckums(mechanism)
        self._ckhms(mechanism)
        self._ckgms(mechanism)
        self._ckams(mechanism)
        self._cksms(mechanism)

        self._ckcpbl(mechanism)
        self._ckcpbs(mechanism)
        self._ckcvbl(mechanism)
        self._ckcvbs(mechanism)
        
        self._ckhbml(mechanism)
        self._ckhbms(mechanism)
        self._ckubml(mechanism)
        self._ckubms(mechanism)
        self._cksbml(mechanism)
        self._cksbms(mechanism)
        self._ckgbml(mechanism)
        self._ckgbms(mechanism)
        self._ckabml(mechanism)
        self._ckabms(mechanism)

        self._ckwc(mechanism)
        self._ckwyp(mechanism)
        self._ckwxp(mechanism)
        self._ckwyr(mechanism)
        self._ckwxr(mechanism)
        
        self._ckqc(mechanism)
        self._ckkfkr(mechanism)
        self._ckqyp(mechanism)
        self._ckqxp(mechanism)
        self._ckqyr(mechanism)
        self._ckqxr(mechanism)

        self._cknu(mechanism)
        self._ckncf(mechanism)
        
        self._ckabe(mechanism)
        
        self._ckeqc(mechanism)
        self._ckeqyp(mechanism)
        self._ckeqxp(mechanism)
        self._ckeqyr(mechanism)
        self._ckeqxr(mechanism)
        
        # Fuego Functions
        self._productionRate(mechanism)
        self._progressRate(mechanism)
        self._progressRateFR(mechanism)
        self._equilibriumConstants(mechanism)
        self._thermo(mechanism)
        self._molecularWeight(mechanism)

        # Fuego extensions
        self._ck_eytt(mechanism)
        self._ck_phity(mechanism)
        self._ck_ytphi(mechanism)
        self._ck_ctyr(mechanism)
        self._ck_cvrhs(mechanism)
        self._ck_cvdim(mechanism)
        self._ck_zndrhs(mechanism)
        self._ck_znddim(mechanism)
        self._ck_mechfile(mechanism)
        self._ck_symnum(mechanism)
        self._ck_symname(mechanism)
        
        
        return


    def _end(self):
        self._timestamp()
        self._rep += self.footer()
        return


    def _includes(self):
        self._rep += [
            '',
            '#include <math.h>',
            '#include <stdio.h>',
            '#include <string.h>',
            '#include <stdlib.h>'
            ]
        return


    def _declarations(self):
        self._rep += [
            '', '',
            '#if defined(BL_FORT_USE_UPPERCASE)',
            '#define CKINDX CKINDX',
            '#define CKINIT CKINIT',
            '#define CKXNUM CKXNUM',
            '#define CKSYME CKSYME',
            '#define CKSYMS CKSYMS',
            '#define CKRP CKRP',
            '#define CKPX CKPX',
            '#define CKPY CKPY',
            '#define CKPC CKPC',
            '#define CKRHOX CKRHOX',
            '#define CKRHOY CKRHOY',
            '#define CKRHOC CKRHOC',
            '#define CKWT CKWT',
            '#define CKMMWY CKMMWY',
            '#define CKMMWX CKMMWX',
            '#define CKMMWC CKMMWC',
            '#define CKYTX CKYTX',
            '#define CKYTCP CKYTCP',
            '#define CKYTCR CKYTCR',
            '#define CKXTY CKXTY',
            '#define CKXTCP CKXTCP',
            '#define CKXTCR CKXTCR',
            '#define CKCTX CKCTX',
            '#define CKCTY CKCTY',
            '#define CKCPOR CKCPOR',
            '#define CKHORT CKHORT',
            '#define CKSOR CKSOR',
            '#define CKCVML CKCVML',
            '#define CKCPML CKCPML',
            '#define CKUML CKUML',
            '#define CKHML CKHML',
            '#define CKGML CKGML',
            '#define CKAML CKAML',
            '#define CKSML CKSML',
            '#define CKCVMS CKCVMS',
            '#define CKCPMS CKCPMS',
            '#define CKUMS CKUMS',
            '#define CKHMS CKHMS',
            '#define CKGMS CKGMS',
            '#define CKAMS CKAMS',
            '#define CKSMS CKSMS',
            '#define CKCPBL CKCPBL',
            '#define CKCPBS CKCPBS',
            '#define CKCVBL CKCVBL',
            '#define CKCVBS CKCVBS',
            '#define CKHBML CKHBML',
            '#define CKHBMS CKHBMS',
            '#define CKUBML CKUBML',
            '#define CKUBMS CKUBMS',
            '#define CKSBML CKSBML',
            '#define CKSBMS CKSBMS',
            '#define CKGBML CKGBML',
            '#define CKGBMS CKGBMS',
            '#define CKABML CKABML',
            '#define CKABMS CKABMS',
            '#define CKWC CKWC',
            '#define CKWYP CKWYP',
            '#define CKWXP CKWXP',
            '#define CKWYR CKWYR',
            '#define CKWXR CKWXR',
            '#define CKQC CKQC',
            '#define CKKFKR CKKFKR',
            '#define CKQYP CKQYP',
            '#define CKQXP CKQXP',
            '#define CKQYR CKQYR',
            '#define CKQXR CKQXR',
            '#define CKNU CKNU',
            '#define CKNCF CKNCF',
            '#define CKABE CKABE',
            '#define CKEQC CKEQC',
            '#define CKEQYP CKEQYP',
            '#define CKEQXP CKEQXP',
            '#define CKEQYR CKEQYR',
            '#define CKEQXR CKEQXR',
            '#elif defined(BL_FORT_USE_LOWERCASE)',
            '#define CKINDX ckindx',
            '#define CKINIT ckinit',
            '#define CKXNUM ckxnum',
            '#define CKSYME cksyme',
            '#define CKSYMS cksyms',
            '#define CKRP ckrp',
            '#define CKPX ckpx',
            '#define CKPY ckpy',
            '#define CKPC ckpc',
            '#define CKRHOX ckrhox',
            '#define CKRHOY ckrhoy',
            '#define CKRHOC ckrhoc',
            '#define CKWT ckwt',
            '#define CKMMWY ckmmwy',
            '#define CKMMWX ckmmwx',
            '#define CKMMWC ckmmwc',
            '#define CKYTX ckytx',
            '#define CKYTCP ckytcp',
            '#define CKYTCR ckytcr',
            '#define CKXTY ckxty',
            '#define CKXTCP ckxtcp',
            '#define CKXTCR ckxtcr',
            '#define CKCTX ckctx',
            '#define CKCTY ckcty',
            '#define CKCPOR ckcpor',
            '#define CKHORT ckhort',
            '#define CKSOR cksor',
            '#define CKCVML ckcvml',
            '#define CKCPML ckcpml',
            '#define CKUML ckuml',
            '#define CKHML ckhml',
            '#define CKGML ckgml',
            '#define CKAML ckaml',
            '#define CKSML cksml',
            '#define CKCVMS ckcvms',
            '#define CKCPMS ckcpms',
            '#define CKUMS ckums',
            '#define CKHMS ckhms',
            '#define CKGMS ckgms',
            '#define CKAMS ckams',
            '#define CKSMS cksms',
            '#define CKCPBL ckcpbl',
            '#define CKCPBS ckcpbs',
            '#define CKCVBL ckcvbl',
            '#define CKCVBS ckcvbs',
            '#define CKHBML ckhbml',
            '#define CKHBMS ckhbms',
            '#define CKUBML ckubml',
            '#define CKUBMS ckubms',
            '#define CKSBML cksbml',
            '#define CKSBMS cksbms',
            '#define CKGBML ckgbml',
            '#define CKGBMS ckgbms',
            '#define CKABML ckabml',
            '#define CKABMS ckabms',
            '#define CKWC ckwc',
            '#define CKWYP ckwyp',
            '#define CKWXP ckwxp',
            '#define CKWYR ckwyr',
            '#define CKWXR ckwxr',
            '#define CKQC ckqc',
            '#define CKKFKR ckkfkr',
            '#define CKQYP ckqyp',
            '#define CKQXP ckqxp',
            '#define CKQYR ckqyr',
            '#define CKQXR ckqxr',
            '#define CKNU cknu',
            '#define CKNCF ckncf',
            '#define CKABE ckabe',
            '#define CKEQC ckeqc',
            '#define CKEQYP ckeqyp',
            '#define CKEQXP ckeqxp',
            '#define CKEQYR ckeqyr',
            '#define CKEQXR ckeqxr',
            '#elif defined(BL_FORT_USE_UNDERSCORE)',
            '#define CKINDX ckindx_',
            '#define CKINIT ckinit_',
            '#define CKXNUM ckxnum_',
            '#define CKSYME cksyme_',
            '#define CKSYMS cksyms_',
            '#define CKRP ckrp_',
            '#define CKPX ckpx_',
            '#define CKPY ckpy_',
            '#define CKPC ckpc_',
            '#define CKRHOX ckrhox_',
            '#define CKRHOY ckrhoy_',
            '#define CKRHOC ckrhoc_',
            '#define CKWT ckwt_',
            '#define CKMMWY ckmmwy_',
            '#define CKMMWX ckmmwx_',
            '#define CKMMWC ckmmwc_',
            '#define CKYTX ckytx_',
            '#define CKYTCP ckytcp_',
            '#define CKYTCR ckytcr_',
            '#define CKXTY ckxty_',
            '#define CKXTCP ckxtcp_',
            '#define CKXTCR ckxtcr_',
            '#define CKCTX ckctx_',
            '#define CKCTY ckcty_',
            '#define CKCPOR ckcpor_',
            '#define CKHORT ckhort_',
            '#define CKSOR cksor_',
            '#define CKCVML ckcvml_',
            '#define CKCPML ckcpml_',
            '#define CKUML ckuml_',
            '#define CKHML ckhml_',
            '#define CKGML ckgml_',
            '#define CKAML ckaml_',
            '#define CKSML cksml_',
            '#define CKCVMS ckcvms_',
            '#define CKCPMS ckcpms_',
            '#define CKUMS ckums_',
            '#define CKHMS ckhms_',
            '#define CKGMS ckgms_',
            '#define CKAMS ckams_',
            '#define CKSMS cksms_',
            '#define CKCPBL ckcpbl_',
            '#define CKCPBS ckcpbs_',
            '#define CKCVBL ckcvbl_',
            '#define CKCVBS ckcvbs_',
            '#define CKHBML ckhbml_',
            '#define CKHBMS ckhbms_',
            '#define CKUBML ckubml_',
            '#define CKUBMS ckubms_',
            '#define CKSBML cksbml_',
            '#define CKSBMS cksbms_',
            '#define CKGBML ckgbml_',
            '#define CKGBMS ckgbms_',
            '#define CKABML ckabml_',
            '#define CKABMS ckabms_',
            '#define CKWC ckwc_',
            '#define CKWYP ckwyp_',
            '#define CKWXP ckwxp_',
            '#define CKWYR ckwyr_',
            '#define CKWXR ckwxr_',
            '#define CKQC ckqc_',
            '#define CKKFKR ckkfkr_',
            '#define CKQYP ckqyp_',
            '#define CKQXP ckqxp_',
            '#define CKQYR ckqyr_',
            '#define CKQXR ckqxr_',
            '#define CKNU cknu_',
            '#define CKNCF ckncf_',
            '#define CKABE ckabe_',
            '#define CKEQC ckeqc_',
            '#define CKEQYP ckeqyp_',
            '#define CKEQXP ckeqxp_',
            '#define CKEQYR ckeqyr_',
            '#define CKEQXR ckeqxr_',
            '#endif','',
            self.line('function declarations'),
            'extern "C" {',
            'void molecularWeight(double * wt);',
            'void gibbs(double * species, double * tc);',
            'void helmholtz(double * species, double * tc);',
            'void speciesInternalEnergy(double * species, double * tc);',
            'void speciesEnthalpy(double * species, double * tc);',
            'void speciesEntropy(double * species, double * tc);',
            'void cp_R(double * species, double * tc);',
            'void cv_R(double * species, double * tc);',
            'void equilibriumConstants(double * kc, double * g_RT, double T);',
            'void productionRate(double * wdot, double * sc, double T);',
            'void progressRate(double * qdot, double * speciesConc, double T);',
            'void progressRateFR(double * q_f, double * q_r, double * speciesConc, double T);',
            'void CKINDX'+sym+'(int * iwrk, double *rwrk, int * mm, int * kk, int * ii, int * nfit );',
            'void CKXNUM'+sym+'(char * line, int * nexp, int * lout, int * nval, double * rval, int * kerr, int lenline);',
            'void CKSNUM'+sym+'(char * line, int * nexp, int * lout, char * kray, int * nn, int * knum, int * nval, double * rval, int * kerr, int lenline, int lenkray);',
            'void CKSYME(int * kname, int * lenkname);',
            'void CKSYMS(int * kname, int * lenkname);',
            #'void CKSYMS'+sym+'(char * cckwrk, int * lout, char * kname, int * kerr, int lencck, int lenkname);',
            'void CKRP'+sym+'(int * ickwrk, double * rckwrk, double * ru, double * ruc, double * pa);',
            'void CKPX'+sym+'(double * rho, double * T, double * x, int * iwrk, double *rwrk, double * P);',
            'void CKPY'+sym+'(double * rho, double * T, double * y, int * iwrk, double *rwrk, double * P);',
            'void CKPC'+sym+'(double * rho, double * T, double * c, int * iwrk, double *rwrk, double * P);',
            'void CKRHOX'+sym+'(double * P, double * T, double * x, int * iwrk, double *rwrk, double * rho);',
            'void CKRHOY'+sym+'(double * P, double * T, double * y, int * iwrk, double *rwrk, double * rho);',
            'void CKRHOC'+sym+'(double * P, double * T, double * c, int * iwrk, double *rwrk, double * rho);',
            'void CKWT'+sym+'(int * iwrk, double *rwrk, double * wt);',
            'void CKMMWY'+sym+'(double * y, int * iwrk, double * rwrk, double * wtm);',
            'void CKMMWX'+sym+'(double * x, int * iwrk, double * rwrk, double * wtm);',
            'void CKMMWC'+sym+'(double * c, int * iwrk, double * rwrk, double * wtm);',
            'void CKYTX'+sym+'(double * y, int * iwrk, double * rwrk, double * x);',
            'void CKYTCP'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * c);',
            'void CKYTCR'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * c);',
            'void CKXTY'+sym+'(double * x, int * iwrk, double * rwrk, double * y);',
            'void CKXTCP'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * c);',
            'void CKXTCR'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * c);',
            'void CKCTX'+sym+'(double * c, int * iwrk, double * rwrk, double * x);',
            'void CKCTY'+sym+'(double * c, int * iwrk, double * rwrk, double * y);',
            'void CKCPOR'+sym+'(double * T, int * iwrk, double * rwrk, double * cpor);',
            'void CKHORT'+sym+'(double * T, int * iwrk, double * rwrk, double * hort);',
            'void CKSOR'+sym+'(double * T, int * iwrk, double * rwrk, double * sor);',
            
            'void CKCVML'+sym+'(double * T, int * iwrk, double * rwrk, double * cvml);',
            'void CKCPML'+sym+'(double * T, int * iwrk, double * rwrk, double * cvml);',
            'void CKUML'+sym+'(double * T, int * iwrk, double * rwrk, double * uml);',
            'void CKHML'+sym+'(double * T, int * iwrk, double * rwrk, double * uml);',
            'void CKGML'+sym+'(double * T, int * iwrk, double * rwrk, double * gml);',
            'void CKAML'+sym+'(double * T, int * iwrk, double * rwrk, double * aml);',
            'void CKSML'+sym+'(double * T, int * iwrk, double * rwrk, double * sml);',
            
            'void CKCVMS'+sym+'(double * T, int * iwrk, double * rwrk, double * cvms);',
            'void CKCPMS'+sym+'(double * T, int * iwrk, double * rwrk, double * cvms);',
            'void CKUMS'+sym+'(double * T, int * iwrk, double * rwrk, double * ums);',
            'void CKHMS'+sym+'(double * T, int * iwrk, double * rwrk, double * ums);',
            'void CKGMS'+sym+'(double * T, int * iwrk, double * rwrk, double * gms);',
            'void CKAMS'+sym+'(double * T, int * iwrk, double * rwrk, double * ams);',
            'void CKSMS'+sym+'(double * T, int * iwrk, double * rwrk, double * sms);',
            
            'void CKCPBL'+sym+'(double * T, double * x, int * iwrk, double * rwrk, double * cpbl);',
            'void CKCPBS'+sym+'(double * T, double * y, int * iwrk, double * rwrk, double * cpbs);',
            'void CKCVBL'+sym+'(double * T, double * x, int * iwrk, double * rwrk, double * cpbl);',
            'void CKCVBS'+sym+'(double * T, double * y, int * iwrk, double * rwrk, double * cpbs);',
            
            'void CKHBML'+sym+'(double * T, double * x, int * iwrk, double * rwrk, double * hbml);',
            'void CKHBMS'+sym+'(double * T, double * y, int * iwrk, double * rwrk, double * hbms);',
            'void CKUBML'+sym+'(double * T, double * x, int * iwrk, double * rwrk, double * ubml);',
            'void CKUBMS'+sym+'(double * T, double * y, int * iwrk, double * rwrk, double * ubms);',
            'void CKSBML'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * sbml);',
            'void CKSBMS'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * sbms);',
            'void CKGBML'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * gbml);',
            'void CKGBMS'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * gbms);',
            'void CKABML'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * abml);',
            'void CKABMS'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * abms);',

            
            'void CKWC'+sym+'(double * T, double * C, int * iwrk, double *rwrk, double * wdot);',
            'void CKWYP'+sym+'(double * P, double * T, double * y, int * iwrk, double *rwrk, double * wdot);',
            'void CKWXP'+sym+'(double * P, double * T, double * x, int * iwrk, double *rwrk, double * wdot);',
            'void CKWYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double *rwrk, double * wdot);',
            'void CKWXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double *rwrk, double * wdot);',

            
            'void CKQC'+sym+'(double * T, double * C, int * iwrk, double *rwrk, double * qdot);',
            'void CKKFKR(double * P, double * T, double * x, int * iwrk, double *rwrk, double * q_f, double * q_r);',
            'void CKQYP'+sym+'(double * P, double * T, double * y, int * iwrk, double *rwrk, double * qdot);',
            'void CKQXP'+sym+'(double * P, double * T, double * x, int * iwrk, double *rwrk, double * qdot);',
            'void CKQYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double *rwrk, double * qdot);',
            'void CKQXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double *rwrk, double * qdot);',
            
            'void CKNU'+sym+'(int * kdim, int * iwrk, double *rwrk, int * nuki);',
            'void CKNCF'+sym+'(int * mdim, int * iwrk, double *rwrk, int * ncf);',
            
            'void CKABE'+sym+'(int * iwrk, double *rwrk, double * a, double * b, double * e );',
            'void CKEQC'+sym+'(double * T, double * C , int * iwrk, double *rwrk, double * eqcon );',
            'void CKEQYP'+sym+'(double * P, double * T, double * y, int * iwrk, double *rwrk, double * eqcon);',
            'void CKEQXP'+sym+'(double * P, double * T, double * x, int * iwrk, double *rwrk, double * eqcon);',
            'void CKEQYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double *rwrk, double * eqcon);',
            'void CKEQXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double *rwrk, double * eqcon);',
            'int  feeytt'+fsym+'(double * e, double * y, int * iwrk, double *rwrk, double * t);',
            'void fephity'+fsym+'(double * phi, int * iwrk, double *rwrk, double * y);',
            'void feytphi'+fsym+'(double * y, int * iwrk, double *rwrk, double * phi);',
            'void fectyr'+fsym+'(double * c, double * rho, int * iwrk, double *rwrk, double * y);',

            'void fecvrhs'+fsym+'(double * time, double * phi, double * phidot, double * rckwrk, int * ickwrk);',
            'int fecvdim'+fsym+'();',
            'void fezndrhs'+fsym+'(double * time, double * z, double * zdot, double * rckwrk, int * ickwrk);',
            'int feznddim'+fsym+'();',
            'char* femechfile'+fsym+'();',
            'char* fesymname'+fsym+'(int sn);',
            'int fesymnum'+fsym+'(const char* s1);',
            '}',
            ]
        return


    def _main(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('optional test program'))
        self._write('int main()')
        self._write('{')
        self._indent()

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        # declarations
        self._write('int species;')
        self._write('int reaction;')

        self._write('double T;')
        self._write('double q_dot[%d];' % nReactions)
        self._write('double wdot[%d];' % nSpecies)
        self._write('double sc[%d];' % nSpecies)
        self._write('double uml[%d];' % nSpecies)
        self._write('double rckdummy[%d];' % nSpecies)
        self._write('int    ickdummy[%d];' % nSpecies)

        # set the temperature
        self._write()
        self._write('T = 1000.0;')

        # compute ckuml 
        self._write()
        self._write(self.line('compute the internal energy'))
        self._write('CKUML(&T, ickdummy, rckdummy, uml);')
        
        # print
        self._write()
        self._write('for (species = 0; species < %d; ++species) {' % nSpecies)
        self._indent()
        self._write('printf(" e: %5d   %15.7e\\n", species+1, uml[species]);')
        self._outdent()
        self._write('}')


        # compute the gibbs free energy
        # self._write()
        # self._write(self.line('compute the Gibbs free energy'))
        # self._write('gibbs(g_RT, T);')

        # compute the equilibrium constants
        # self._write()
        # self._write(self.line('compute the equilibrium constants'))
        # self._write('equilibriumConstants(kc, g_RT, T);')

        self._write('for (species = 0; species < %d; ++species) {' % nSpecies)
        self._indent()
        self._write('sc[species] = 1.0e6;')
        self._outdent()
        self._write('}')

        # compute the production rates
        self._write()
        self._write(self.line('compute the production rate'))
        self._write('productionRate(wdot, sc, T);')

        # compute the progress rates
        # self._write()
        # self._write(self.line('compute the progress rates'))
        # self._write('progressRate(q_dot, sc, T);')

        # print
        self._write()
        self._write('for (species = 0; species < %d; ++species) {' % nSpecies)
        self._indent()
        self._write('printf("%5d   %15.7e\\n", species+1, wdot[species]);')
        self._outdent()
        self._write('}')

        # print
        # self._write()
        # self._write('for (reaction = 0; reaction < %d; ++reaction) {' % nReactions)
        # self._indent()
        # self._write('printf("%5d | %15.7e\\n", reaction+1, q_dot[reaction]);')
        # self._write('}')
        # self._outdent()

        # done
        self._write()
        self._write('return 0;')

        self._outdent()
        self._write('}')
        return


    def _thermo(self, mechanism):
        speciesInfo = self._analyzeThermodynamics(mechanism)

        self._gibbs(speciesInfo)
        self._helmholtz(speciesInfo)
        self._cv(speciesInfo)
        self._cp(speciesInfo)
        self._speciesInternalEnergy(speciesInfo)
        self._speciesEnthalpy(speciesInfo)
        self._speciesEntropy(speciesInfo)

        return


    def _ckxnum(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(' ckxnum... for parsing strings '))
        self._write('void CKXNUM'+sym+'(char * line, int * nexp, int * lout, int * nval, double * rval, int * kerr, int lenline )')
        self._write('{')
        self._indent()

        self._write('int n,i; ' + self.line('Loop Counters'))
        self._write('char *p; ' + self.line('String Tokens'))
        self._write('char cstr[1000];')

        self._write(self.line(' Strip Comments '))
        self._write('for (i=0; i<lenline; ++i) {')
        self._indent()
        self._write('if (line[i]==\'!\') {')
        self._indent()
        self._write('cstr[i] = \'\\0\';')
        self._write('break;')
        self._outdent()
        self._write('}')
        self._write('cstr[i] = line[i];')
        self._outdent()
        self._write('}')

        self._write()
        self._write('p = strtok(cstr," ");')
        self._write('if (!p) {')
        self._indent()
        self._write('*nval = 0;')
        self._write('*kerr = 1;')
        self._write('return;')
        self._outdent()
        self._write('}')

        self._write('for (n=0; n<*nexp; ++n) {')
        self._indent()
        self._write('rval[n] = atof(p);')
        self._write('p = strtok(NULL, " ");');
        self._write('if (!p) break;')
        self._outdent()
        self._write('}')
        self._write('*nval = n+1;')
        self._write('if (*nval < *nexp) *kerr = 1;')
        self._write('return;')
        
                   
        # done
        self._outdent()
        self._write('}')
        return


    def _cksnum(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(' cksnum... for parsing strings '))
        self._write('void CKSNUM'+sym+'(char * line, int * nexp, int * lout, char * kray, int * nn, int * knum, int * nval, double * rval, int * kerr, int lenline, int lenkray)')
        self._write('{')
        self._indent()
        
        self._write(self.line('Not done yet ...'))
        
        # done
        self._outdent()
        self._write('}')
        return

    def _ckrp(self, mechanism):
        self._write()
        self._write()
        self._write(
            self.line(' Returns R, Rc, Patm' ))
        self._write('void CKRP'+sym+'(int * ickwrk, double * rckwrk, double * ru, double * ruc, double * pa)')
        self._write('{')
        self._indent()
        
        self._write(' *ru  = %g; ' % (R * mole * kelvin / erg))
        self._write(' *ruc = %.20f; ' % (Rc * mole * kelvin / cal))
        self._write(' *pa  = %g; ' % (Patm) )
        
        # done
        self._outdent()
        self._write('}')
        return

    def _cksyme(self, mechanism):

        nElement = len(mechanism.element())
        
        self._write()
        self._write()
        self._write(
            self.line(' Returns the char strings of element names'))
        self._write('void CKSYME'+sym+'(int * kname, int * plenkname )')
        self._write('{')
        self._indent()

        self._write('int i; '+self.line('Loop Counter'))
        self._write('int lenkname = *plenkname;')
        self._write(self.line('clear kname'))
        self._write('for (i=0; i<lenkname*%d; i++) {' % nElement)
        self._indent()
        self._write('kname[i] = \' \';')
        self._outdent()
        self._write('}')
        self._write()
        for element in mechanism.element():
            self._write(self.line(' %s ' % element.symbol))
            ii = 0
            for char in element.symbol:
                self._write('kname[ %d*lenkname + %d ] = \'%s\';' %
                           (element.id, ii, char.capitalize()))
                ii = ii+1
            self._write('kname[ %d*lenkname + %d ] = \' \';' %
                           (element.id, ii))
            self._write()
            
        # done
        self._outdent()
        self._write('}')
        return


    def _cksyms(self, mechanism):

        nSpecies = len(mechanism.species())
        
        self._write()
        self._write()
        self._write(
            self.line(' Returns the char strings of species names'))
        #self._write('void CKSYMS'+sym+'(char * cckwrk, int * lout, char * kname, int * kerr, int lencck, int lenkname )')
        self._write('void CKSYMS'+sym+'(int * kname, int * plenkname )')
        self._write('{')
        self._indent()
        
        self._write('int i; '+self.line('Loop Counter'))
        self._write('int lenkname = *plenkname;')
        self._write(self.line('clear kname'))
        self._write('for (i=0; i<lenkname*%d; i++) {' % nSpecies)
        self._indent()
        self._write('kname[i] = \' \';')
        self._outdent()
        self._write('}')
        self._write()
        for species in mechanism.species():
            self._write(self.line(' %s ' % species.symbol))
            ii = 0
            for char in species.symbol:
                self._write('kname[ %d*lenkname + %d ] = \'%s\';' %
                           (species.id, ii, char.capitalize()))
                ii = ii+1
            self._write('kname[ %d*lenkname + %d ] = \' \';' %
                           (species.id, ii))
            self._write()

        # done
        self._outdent()
        self._write('}')
        return


    def _ckinit(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Dummy ckinit'))
        self._write('void fginit'+fsym+'(int * leniwk, int * lenrwk, int * lencwk, int * linc, int * lout, int * ickwrk, double * rckwrk, char * cckwrk )')
        self._write('{')
        self._indent()
        self._write('if ((*lout) != 0) {')
        self._indent()
        self._write('printf(" ***       Congratulations       *** \\n");')
        self._write('printf(" * You are using the Fuego Library * \\n");')
        self._write('printf(" *****    Say NO to cklib.f    ***** \\n");')
        self._outdent()
        self._write('}')
        
        # done
        self._outdent()
        self._write('}')
        return
    

    def _ckindx(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('A few mechanism parameters'))
        self._write('void CKINDX'+sym+'(int * iwrk, double * rwrk, int * mm, int * kk, int * ii, int * nfit)')
        self._write('{')
        self._indent()
        self._write('*mm = %d;' % len(mechanism.element()))
        self._write('*kk = %d;' % len(mechanism.species()))
        self._write('*ii = %d;' % len(mechanism.reaction()))
        self._write('*nfit = -1; ' + self.line(
            'Why do you need this anyway ? '))
        
        # done
        self._outdent()
        self._write('}')
        return
        
        
    def _ckpx(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute P = rhoRT/W(x)'))
        self._write('void CKPX'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * P)')
        self._write('{')
        self._indent()

        self._write('double XW = 0;'+
                    self.line(' To hold mean molecular wt'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write(
            '*P = *rho * %g * (*T) / XW; ' % (R*kelvin*mole/erg)
            + self.line('P = rho*R*T/W'))
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')
        return

    def _ckpy(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute P = rhoRT/W(y)'))
        self._write('void CKPY'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * P)')
        self._write('{')
        self._indent()

        self._write('double YOW = 0;'+self.line(' for computing mean MW'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self.line('YOW holds the reciprocal of the mean molecular wt')
        self._write(
            '*P = *rho * %g * (*T) * YOW; ' % (R*kelvin*mole/erg)
            + self.line('P = rho*R*T/W'))
        
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckpc(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute P = rhoRT/W(c)'))
        self._write('void CKPC'+sym+'(double * rho, double * T, double * c, int * iwrk, double * rwrk, double * P)')
        
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write(self.line('See Eq 5 in CK Manual'))
        self._write('double W = 0;')
        self._write('double sumC = 0;')
        
        # molecular weights of all species
        for species in self.species:
            self._write('W += c[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write()
        nSpecies = len(mechanism.species())
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('sumC += c[id];')
        self._outdent()
        self._write('}')

        self.line('W/sumC holds the mean molecular wt')
        self._write(
            '*P = *rho * %g * (*T) * sumC / W; ' % (R*kelvin*mole/erg)
            + self.line('P = rho*R*T/W'))
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 

    def _ckrhox(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute rho = PW(x)/RT'))
        self._write('void CKRHOX'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * rho)')
        self._write('{')
        self._indent()

        self._write('double XW = 0;'+
                    self.line(' To hold mean molecular wt'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write(
            '*rho = *P * XW / (%g * (*T)); ' % (R*kelvin*mole/erg)
            + self.line('rho = P*W/(R*T)'))
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')
        return

    def _ckrhoy(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute rho = P*W(y)/RT'))
        self._write('void CKRHOY'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * rho)')
        self._write('{')
        self._indent()

        self._write('double YOW = 0;'+self.line(' for computing mean MW'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self.line('YOW holds the reciprocal of the mean molecular wt')
        self._write(
            '*rho = *P / (%g * (*T) * YOW); ' % (R*kelvin*mole/erg)
            + self.line('rho = P*W/(R*T)'))
        
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckrhoc(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Compute rho = P*W(c)/(R*T)'))
        self._write('void CKRHOC'+sym+'(double * P, double * T, double * c, int * iwrk, double * rwrk, double * rho)')
        
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write(self.line('See Eq 5 in CK Manual'))
        self._write('double W = 0;')
        self._write('double sumC = 0;')
        
        # molecular weights of all species
        for species in self.species:
            self._write('W += c[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write()
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('sumC += c[id];')
        self._outdent()
        self._write('}')

        self.line('W/sumC holds the mean molecular wt')
        self._write(
            '*rho = *P * W / (sumC * (*T) * %g); ' % (R*kelvin*mole/erg)
            + self.line('rho = PW/(R*T)'))
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 

    def _ckwt(self, mechanism):

        self._write()
        self._write()
        self._write(self.line('get molecular weight for all species'))
        self._write('void CKWT'+sym+'(int * iwrk, double * rwrk, double * wt)')
        self._write('{')
        self._indent()

        # call moleuclarWeight
        self._write('molecularWeight(wt);')
        
        self._outdent()

        self._write('}')

        return
      
    def _ckcvml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get specific heat at constant volume as a function '))
        self._write(self.line('of T for all species (molar units)'))
        self._write('void CKCVML'+sym+'(double *T, int * iwrk, double * rwrk, double * cvml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('cv_R(cvml, tc);')
        
        # convert cv/R to cv
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('cvml[id] *= %g;' % (R*kelvin*mole/erg) )
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
       
    def _ckcpml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get specific heat at constant pressure as a '))
        self._write(self.line('function of T for all species (molar units)'))
        self._write('void CKCPML'+sym+'(double *T, int * iwrk, double * rwrk, double * cpml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('cp_R(cpml, tc);')
        
        # convert cp/R to cp
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('cpml[id] *= %g;' % (R*kelvin*mole/erg) )
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
     
    def _ckuml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get internal energy as a function '))
        self._write(self.line('of T for all species (molar units)'))
        self._write('void CKUML'+sym+'(double *T, int * iwrk, double * rwrk, double * uml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesInternalEnergy(uml, tc);')
        
        # convert e/RT to e with molar units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('uml[id] *= RT;')
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
      
    def _ckhml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get enthalpy as a function '))
        self._write(self.line('of T for all species (molar units)'))
        self._write('void CKHML'+sym+'(double *T, int * iwrk, double * rwrk, double * hml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesEnthalpy(hml, tc);')
        
        # convert h/RT to h with molar units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('hml[id] *= RT;')
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
    
    def _ckgml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get standard-state Gibbs energy as a function '))
        self._write(self.line('of T for all species (molar units)'))
        self._write('void CKGML'+sym+'(double *T, int * iwrk, double * rwrk, double * gml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('gibbs(gml, tc);')
        
        # convert g/RT to g with molar units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('gml[id] *= RT;')
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
    
    def _ckaml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get standard-state Helmholtz free energy as a '))
        self._write(self.line('function of T for all species (molar units)'))
        self._write('void CKAML'+sym+'(double *T, int * iwrk, double * rwrk, double * aml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('helmholtz(aml, tc);')
        
        # convert A/RT to A with molar units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('aml[id] *= RT;')
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
   
    def _cksml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the standard-state entropies in molar units'))
        self._write('void CKSML'+sym+'(double *T, int * iwrk, double * rwrk, double * sml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('speciesEntropy(sml, tc);')
        
        # convert s/R to s
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('sml[id] *= %g;' % (R*kelvin*mole/erg) )
        self._outdent()
        self._write('}')
       
        self._outdent()

        self._write('}')

        return
 
    def _ckums(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns internal energy in mass units (Eq 30.)'))
        self._write('void CKUMS'+sym+'(double *T, int * iwrk, double * rwrk, double * ums)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesInternalEnergy(ums, tc);')
        

        # convert e/RT to e with mass units
        for species in self.species:
            self._write('ums[%d] *= RT/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return
 
    def _ckhms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns enthalpy in mass units (Eq 27.)'))
        self._write('void CKHMS'+sym+'(double *T, int * iwrk, double * rwrk, double * hms)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesEnthalpy(hms, tc);')
        

        # convert h/RT to h with mass units
        for species in self.species:
            self._write('hms[%d] *= RT/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return

    def _ckams(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns helmholtz in mass units (Eq 32.)'))
        self._write('void CKAMS'+sym+'(double *T, int * iwrk, double * rwrk, double * ams)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('helmholtz(ams, tc);')
        

        # convert A/RT to A with mass units
        for species in self.species:
            self._write('ams[%d] *= RT/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return

    def _ckgms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns gibbs in mass units (Eq 31.)'))
        self._write('void CKGMS'+sym+'(double *T, int * iwrk, double * rwrk, double * gms)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('gibbs(gms, tc);')
        

        # convert g/RT to g with mass units
        for species in self.species:
            self._write('gms[%d] *= RT/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return


    def _ckcvms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the specific heats at constant volume'))
        self._write(self.line('in mass units (Eq. 29)'))
        self._write('void CKCVMS'+sym+'(double *T, int * iwrk, double * rwrk, double * cvms)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('cv_R(cvms, tc);')

        # convert cv/R to cv with mass units
        self._write(self.line('multiply by R/molecularweight'))
        for species in self.species:
            ROW = (R*kelvin*mole/erg) / species.weight
            self._write('cvms[%d] *= %20.15e; ' % (
                species.id, ROW) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return

    def _ckcpms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the specific heats at constant pressure'))
        self._write(self.line('in mass units (Eq. 26)'))
        self._write('void CKCPMS'+sym+'(double *T, int * iwrk, double * rwrk, double * cpms)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('cp_R(cpms, tc);')
        

        # convert cp/R to cp with mass units
        self._write(self.line('multiply by R/molecularweight'))
        for species in self.species:
            ROW = (R*kelvin*mole/erg) / species.weight
            self._write('cpms[%d] *= %20.15e; ' % (
                species.id, ROW) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return

    def _cksms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the entropies in mass units (Eq 28.)'))
        self._write('void CKSMS'+sym+'(double *T, int * iwrk, double * rwrk, double * sms)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('speciesEntropy(sms, tc);')
        

        # convert s/R to s with mass units
        self._write(self.line('multiply by R/molecularweight'))
        for species in self.species:
            ROW = (R*kelvin*mole/erg) / species.weight
            self._write('sms[%d] *= %20.15e; ' % (
                species.id, ROW) + self.line('%s' % species.symbol))

       
        self._outdent()

        self._write('}')

        return
    
    def _ckcpbl(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the mean specific heat at CP (Eq. 33)'))
        self._write('void CKCPBL'+sym+'(double *T, double *x, int * iwrk, double * rwrk, double * cpbl)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double cpor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write('cp_R(cpor, tc);')
        
        # dot product
        self._write()
        self._write(self.line('perform dot product'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*cpor[id];')
        self._outdent()
        self._write('}')

        self._write()
        self._write('*cpbl = result * %g;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return
 
    def _ckcpbs(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the mean specific heat at CP (Eq. 34)'))
        self._write('void CKCPBS'+sym+'(double *T, double *y, int * iwrk, double * rwrk, double * cpbs)')
        self._write('{')
        self._indent()

        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double cpor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write('cp_R(cpor, tc);')
        
        # do dot product
        self._write(self.line('multiply by y/molecularweight'))
        for species in self.species:
            self._write('result += cpor[%d]*y[%d]/%f; ' % (
                species.id, species.id, species.weight) + self.line('%s' % species.symbol))

        self._write()
        self._write('*cpbs = result * %g;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return
   
    def _ckcvbl(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the mean specific heat at CV (Eq. 35)'))
        self._write('void CKCVBL'+sym+'(double *T, double *x, int * iwrk, double * rwrk, double * cvbl)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double cvor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write('cv_R(cvor, tc);')
        
        # dot product
        self._write()
        self._write(self.line('perform dot product'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*cvor[id];')
        self._outdent()
        self._write('}')

        self._write()
        self._write('*cvbl = result * %g;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return

    def _ckcvbs(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the mean specific heat at CV (Eq. 36)'))
        self._write('void CKCVBS'+sym+'(double *T, double *y, int * iwrk, double * rwrk, double * cvbs)')
        self._write('{')
        self._indent()

        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double cvor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write('cv_R(cvor, tc);')
        
        # do dot product
        self._write(self.line('multiply by y/molecularweight'))
        for species in self.species:
            self._write('result += cvor[%d]*y[%d]/%f; ' % (
                species.id, species.id, species.weight) + self.line('%s' % species.symbol))

        self._write()
        self._write('*cvbs = result * %g;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return
    
    def _ckhbml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the mean enthalpy of the mixture in molar units'))
        self._write('void CKHBML'+sym+'(double *T, double *x, int * iwrk, double * rwrk, double * hbml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double hml[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesEnthalpy(hml, tc);')
        
        # dot product
        self._write()
        self._write(self.line('perform dot product'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*hml[id];')
        self._outdent()
        self._write('}')

        self._write()
        self._write('*hbml = result * RT;')
        
        self._outdent()

        self._write('}')

        return
 
 
    def _ckhbms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns mean enthalpy of mixture in mass units'))
        self._write('void CKHBMS'+sym+'(double *T, double *y, int * iwrk, double * rwrk, double * hbms)')
        self._write('{')
        self._indent()

        self._write('double result = 0;')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double hml[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesEnthalpy(hml, tc);')

        # convert e/RT to e with mass units
        self._write(self.line('perform dot product + scaling by wt'))
        for species in self.species:
            self._write('result += y[%d]*hml[%d]/%f; ' % (
                species.id, species.id, species.weight)
                        + self.line('%s' % species.symbol))

        
        self._write()
        # finally, multiply by RT
        self._write('*hbms = result * RT;')
        
        self._outdent()

        self._write('}')
        
        return
    
    def _ckubml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get mean internal energy in molar units'))
        self._write('void CKUBML'+sym+'(double *T, double *x, int * iwrk, double * rwrk, double * ubml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double uml[%d]; ' % self.nSpecies + self.line(' temporary energy array'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesInternalEnergy(uml, tc);')
        
        # dot product
        self._write()
        self._write(self.line('perform dot product'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*uml[id];')
        self._outdent()
        self._write('}')

        self._write()
        self._write('*ubml = result * RT;')
        
        self._outdent()

        self._write('}')

        return
 
    def _ckubms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get mean internal energy in mass units'))
        self._write('void CKUBMS'+sym+'(double *T, double *y, int * iwrk, double * rwrk, double * ubms)')
        self._write('{')
        self._indent()

        self._write('double result = 0;')
        
        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double ums[%d]; ' % self.nSpecies + self.line(' temporary energy array'))
        
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        
        # call routine
        self._write('speciesInternalEnergy(ums, tc);')

        # convert e/RT to e with mass units
        self._write(self.line('perform dot product + scaling by wt'))
        for species in self.species:
            self._write('result += y[%d]*ums[%d]/%f; ' % (
                species.id, species.id, species.weight)
                        + self.line('%s' % species.symbol))

        
        self._write()
        # finally, multiply by RT
        self._write('*ubms = result * RT;')
        
        self._outdent()

        self._write('}')
        
        return
 
    def _cksbml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get mixture entropy in molar units'))
        self._write('void CKSBML'+sym+'(double *P, double *T, double *x, int * iwrk, double * rwrk, double * sbml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double sor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        
        # call routine
        self._write('speciesEntropy(sor, tc);')
        
        # Equation 42
        self._write()
        self._write(self.line('Compute Eq 42'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*(sor[id]-log((x[id]+%g))-logPratio);' %
                    smallnum )
        self._outdent()
        self._write('}')

        self._write()
        
        self._write('*sbml = result * %g;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return

    def _cksbms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get mixture entropy in mass units'))
        self._write('void CKSBMS'+sym+'(double *P, double *T, double *y, int * iwrk, double * rwrk, double * sbms)')
        self._write('{')
        self._indent()

        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double sor[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        self._write(
            'double x[%d]; ' % self.nSpecies + self.line(' need a ytx conversion'))

        self._write('double YOW = 0; '+self.line('See Eq 4, 6 in CK Manual'))
        
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        # now to ytx
        self._write(self.line('Now compute y to x conversion'))
        for species in self.species:
            self._write('x[%d] = y[%d]/(%f*YOW); ' % (
                species.id, species.id, species.weight) )
            
        # call routine
        self._write('speciesEntropy(sor, tc);')
        
        # Equation 42 and 43
        self._write(self.line('Perform computation in Eq 42 and 43'))
        for species in self.species:
            self._write('result += x[%d]*(sor[%d]-log((x[%d]+%g))-logPratio);' %
                        (species.id, species.id, species.id, smallnum) )

        self._write(self.line('Scale by R/W'))
        self._write('*sbms = result * %g * YOW;' % (R*kelvin*mole/erg) )
        
        self._outdent()

        self._write('}')

        return

    def _ckgbml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns mean gibbs free energy in molar units'))
        self._write('void CKGBML'+sym+'(double *P, double *T, double *x, int * iwrk, double * rwrk, double * gbml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        self._write(
            'double gort[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write(self.line('Compute g/RT'))
        self._write('gibbs(gort, tc);')
        
        # Equation 44
        self._write()
        self._write(self.line('Compute Eq 44'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*(gort[id]+log((x[id]+%g))+logPratio);' %
                    smallnum )
        self._outdent()
        self._write('}')

        self._write()
        
        self._write('*gbml = result * RT;')
        
        self._outdent()

        self._write('}')

        return


    def _ckgbms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns mixture gibbs free energy in mass units'))
        self._write('void CKGBMS'+sym+'(double *P, double *T, double *y, int * iwrk, double * rwrk, double * gbms)')
        self._write('{')
        self._indent()

        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        self._write(
            'double gort[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        self._write(
            'double x[%d]; ' % self.nSpecies + self.line(' need a ytx conversion'))

        self._write(
            'double YOW = 0; '
            + self.line('To hold 1/molecularweight'))
        
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        # now to ytx
        self._write(self.line('Now compute y to x conversion'))
        for species in self.species:
            self._write('x[%d] = y[%d]/(%f*YOW); ' % (
                species.id, species.id, species.weight) )
            
        # call routine
        self._write('gibbs(gort, tc);')
        
        # Equation 42 and 43
        self._write(self.line('Perform computation in Eq 44'))
        for species in self.species:
            self._write('result += x[%d]*(gort[%d]+log((x[%d]+%g))+logPratio);' %
                        (species.id, species.id, species.id, smallnum) )

        self._write(self.line('Scale by RT/W'))
        self._write('*gbms = result * RT * YOW;')
        
        self._outdent()

        self._write('}')

        return
    

    def _ckabml(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns mean helmholtz free energy in molar units'))
        self._write('void CKABML'+sym+'(double *P, double *T, double *x, int * iwrk, double * rwrk, double * abml)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        self._write(
            'double aort[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        
        # call routine
        self._write(self.line('Compute g/RT'))
        self._write('helmholtz(aort, tc);')
        
        # Equation 44
        self._write()
        self._write(self.line('Compute Eq 44'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('result += x[id]*(aort[id]+log((x[id]+%g))+logPratio);' %
                    smallnum )
        self._outdent()
        self._write('}')

        self._write()
        
        self._write('*abml = result * RT;')
        
        self._outdent()

        self._write('}')

        return
    

    def _ckabms(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns mixture helmholtz free energy in mass units'))
        self._write('void CKABMS'+sym+'(double *P, double *T, double *y, int * iwrk, double * rwrk, double * abms)')
        self._write('{')
        self._indent()

        self._write('double result = 0; ')
        
        # get temperature cache
        self._write(self.line('Log of normalized pressure in cgs units dynes/cm^2 by Patm'))
        self._write( 'double logPratio = log ( *P / 1013250.0 ); ')
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double RT = %g*tT; ' % (R*kelvin*mole/erg)
            + self.line('R*T'))
        self._write(
            'double aort[%d]; ' % self.nSpecies + self.line(' temporary storage'))
        self._write(
            'double x[%d]; ' % self.nSpecies + self.line(' need a ytx conversion'))

        self._write(
            'double YOW = 0; '
            + self.line('To hold 1/molecularweight'))
        
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        # now to ytx
        self._write(self.line('Now compute y to x conversion'))
        for species in self.species:
            self._write('x[%d] = y[%d]/(%f*YOW); ' % (
                species.id, species.id, species.weight) )
            
        # call routine
        self._write('helmholtz(aort, tc);')
        
        # Equation 42 and 43
        self._write(self.line('Perform computation in Eq 44'))
        for species in self.species:
            self._write('result += x[%d]*(aort[%d]+log((x[%d]+%g))+logPratio);' %
                        (species.id, species.id, species.id, smallnum) )

        self._write(self.line('Scale by RT/W'))
        self._write('*abms = result * RT * YOW;')
        
        self._outdent()

        self._write('}')

        return
    

    def _ckwc(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('compute the production rate for each species'))
        self._write('void CKWC'+sym+'(double * T, double * C, int * iwrk, double * rwrk, double * wdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        # convert C to SI units
        self._write()
        self._write(self.line('convert to SI'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('C[id] *= 1.0e6;')
        self._outdent()
        self._write('}')
        
        # call productionRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('productionRate(wdot, C, *T);')

        # convert C and wdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('C[id] *= 1.0e-6;')
        self._write('wdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return

    def _ckwyp(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the molar production rate of species'))
        self._write(self.line('Given P, T, and mass fractions'))
        self._write('void CKWYP'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * wdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % self.nSpecies + self.line('temporary storage'))
        self._write('double YOW = 0; ')
        self._write('double PWORT; ')
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        self._write(self.line('PW/RT (see Eq. 7)'))
        self._write('PWORT = (*P)/(YOW * %g * (*T)); ' % (R*kelvin*mole/erg) )
        
        self._write(self.line('multiply by 1e6 so c goes to SI'))
        self._write('PWORT *= 1e6; ')

        # now compute conversion
        self._write(self.line('Now compute conversion (and go to SI)'))
        for species in self.species:
            self._write('c[%d] = PWORT * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )

        # call productionRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('productionRate(wdot, c, *T);')

        # convert wdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('wdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckwxp(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the molar production rate of species'))
        self._write(self.line('Given P, T, and mole fractions'))
        self._write('void CKWXP'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * wdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % self.nSpecies + self.line('temporary storage'))
        
        self._write('double PORT = 1e6 * (*P)/(%g * (*T)); ' % (R*kelvin*mole/erg) +
                    self.line('1e6 * P/RT so c goes to SI units'))
        
        # now compute conversion
        self._write()
        self._write(self.line('Compute conversion, see Eq 10'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('c[id] = x[id]*PORT;')
        self._outdent()
        self._write('}')
        
        # call productionRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('productionRate(wdot, c, *T);')

        # convert wdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('wdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckwyr(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the molar production rate of species'))
        self._write(self.line('Given rho, T, and mass fractions'))
        self._write('void CKWYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * wdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % self.nSpecies + self.line('temporary storage'))

        # now compute conversion
        self._write(self.line('See Eq 8 with an extra 1e6 so c goes to SI'))
        for species in self.species:
            self._write('c[%d] = 1e6 * (*rho) * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )
            
        # call productionRate
        self._write()
        self._write(self.line('call productionRate'))
        self._write('productionRate(wdot, c, *T);')

        # convert wdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('wdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckwxr(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('Returns the molar production rate of species'))
        self._write(self.line('Given rho, T, and mole fractions'))
        self._write('void CKWXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * wdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % self.nSpecies + self.line('temporary storage'))
        
        self._write('double XW = 0; '+self.line('See Eq 4, 11 in CK Manual'))
        self._write('double ROW; ')
        
        # compute mean molecular weight first (eq 3)
        self._write(self.line('Compute mean molecular wt first'))
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        # now compute conversion
        self._write(self.line('Extra 1e6 factor to take c to SI'))
        self._write('ROW = 1e6*(*rho) / XW;')
        self._write()
        self._write(self.line('Compute conversion, see Eq 11'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('c[id] = x[id]*ROW;')
        self._outdent()
        self._write('}')
        
        # call productionRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('productionRate(wdot, c, *T);')

        # convert wdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('wdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _cknu(self, mechanism):

        nSpecies  = len(mechanism.species())
        nReaction = len(mechanism.reaction())

        self._write()
        self._write()
        self._write(self.line('Returns the stoichiometric coefficients'))
        self._write(self.line('of the reaction mechanism. (Eq 50)'))
        self._write('void CKNU'+sym+'(int * kdim, int * iwrk, double * rwrk, int * nuki)')
        self._write('{')
        self._indent()

 
        self._write('int id; ' + self.line('loop counter'))
        self._write('int kd = (*kdim); ')
        self._write(self.line('Zero nuki'))
        self._write('for (id = 0; id < %d * kd; ++ id) {' % (nSpecies) )
        self._indent()
        self._write(' nuki[id] = 0; ')
        self._outdent()
        self._write('}')
        
        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            for symbol, coefficient in reaction.reactants:
                self._write(
                    "nuki[ %d * kd + %d ] += -%d ;"
                    % (mechanism.species(symbol).id, reaction.id-1, coefficient))

            for symbol, coefficient in reaction.products:
                self._write(
                    "nuki[ %d * kd + %d ] += +%d ;"
                    % (mechanism.species(symbol).id, reaction.id-1, coefficient))
       
        # done
        self._outdent()
        self._write('}')

        return


    def _ckncf(self, mechanism):

        nSpecies  = len(mechanism.species())
        nElement  = len(mechanism.element())

        self._write()
        self._write()
        self._write(self.line('Returns the elemental composition '))
        self._write(self.line('of the speciesi (mdim is num of elements)'))
        self._write('void CKNCF'+sym+'(int * mdim, int * iwrk, double * rwrk, int * ncf)')
        self._write('{')
        self._indent()

 
        self._write('int id; ' + self.line('loop counter'))
        self._write('int kd = (*mdim); ')
        self._write(self.line('Zero ncf'))
        self._write('for (id = 0; id < %d * %d; ++ id) {' % (nElement, self.nSpecies) )
        self._indent()
        self._write(' ncf[id] = 0; ')
        self._outdent()
        self._write('}')
        
        self._write()
        for species in mechanism.species():
           self._write(self.line('%s' % species.symbol))
           for elem, coef in species.composition:
               self._write('ncf[ %d * kd + %d ] = %d; ' % (
                   species.id, mechanism.element(elem).id, coef) +
                       self.line('%s' % elem) )
                           
           self._write()
                            
        # done
        self._outdent()

        self._write('}')

        return


    def _ckabe(self, mechanism):

        nElement  = len(mechanism.element())

        self._write()
        self._write()
        self._write(self.line('Returns the arrehenius coefficients '))
        self._write(self.line('for all reactions'))
        self._write('void CKABE'+sym+'(int * iwrk, double * rwrk, double * a, double * b, double * e)')
        self._write('{')
        self._indent()

 
        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            # store the progress rate
            self._write("a[%d] = %g;" % (reaction.id-1 , reaction.arrhenius[0]))
            self._write("b[%d] = %g;" % (reaction.id-1 , reaction.arrhenius[1]))
            self._write("e[%d] = %.10g;" % (reaction.id-1 , reaction.arrhenius[2]))

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return
                            
        # done
        self._outdent()

        self._write('}')

        return

    
    def _ckmmwy(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('given y[species]: mass fractions'))
        self._write(self.line('returns mean molecular weight (gm/mole)'))
        self._write('void CKMMWY'+sym+'(double *y, int * iwrk, double * rwrk, double * wtm)')
        self._write('{')
        self._indent()

        self._write('double YOW = 0;'+self.line(' see Eq 3 in CK Manual'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write('*wtm = 1.0 / YOW;')
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckmmwx(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('given x[species]: mole fractions'))
        self._write(self.line('returns mean molecular weight (gm/mole)'))
        self._write('void CKMMWX'+sym+'(double *x, int * iwrk, double * rwrk, double * wtm)')
        self._write('{')
        self._indent()

        self._write('double XW = 0;'+self.line(' see Eq 4 in CK Manual'))
        
        # molecular weights of all species
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write('*wtm = XW;')
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckmmwc(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('given c[species]: molar concentration'))
        self._write(self.line('returns mean molecular weight (gm/mole)'))
        self._write('void CKMMWC'+sym+'(double *c, int * iwrk, double * rwrk, double * wtm)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write(self.line('See Eq 5 in CK Manual'))
        self._write('double W = 0;')
        self._write('double sumC = 0;')
        
        # molecular weights of all species
        for species in self.species:
            self._write('W += c[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        self._write()
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('sumC += c[id];')
        self._outdent()
        self._write('}')

        self._write(self.line(' CK provides no guard against divison by zero'))
        self._write('*wtm = W/sumC;')
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckytx(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert y[species] (mass fracs) to x[species] (mole fracs)'))
        self._write('void CKYTX'+sym+'(double * y, int * iwrk, double * rwrk, double * x)')
        self._write('{')
        self._indent()

        self._write('double YOW = 0; '+self.line('See Eq 4, 6 in CK Manual'))
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        # now compute conversion
        self._write(self.line('Now compute conversion'))
        for species in self.species:
            self._write('x[%d] = y[%d]/(%f*YOW); ' % (
                species.id, species.id, species.weight) )

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckytcp(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert y[species] (mass fracs) to c[species] (molar conc)'))
        self._write('void CKYTCP'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * c)')
        self._write('{')
        self._indent()

        self._write('double YOW = 0; ')
        self._write('double PWORT; ')
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        self._write(self.line('PW/RT (see Eq. 7)'))
        self._write('PWORT = (*P)/(YOW * %g * (*T)); ' % (R*kelvin*mole/erg) )

        # now compute conversion
        self._write(self.line('Now compute conversion'))
        for species in self.species:
            self._write('c[%d] = PWORT * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckytcr(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert y[species] (mass fracs) to c[species] (molar conc)'))
        self._write('void CKYTCR'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * c)')
        self._write('{')
        self._indent()

        # now compute conversion
        self._write(self.line('See Eq 8 (Temperature not used)'))
        for species in self.species:
            self._write('c[%d] = (*rho) * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckxty(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert x[species] (mole fracs) to y[species] (mass fracs)'))
        self._write('void CKXTY'+sym+'(double * x, int * iwrk, double * rwrk, double * y)')
        self._write('{')
        self._indent()

        self._write('double XW = 0; '+self.line('See Eq 4, 9 in CK Manual'))
        
        # compute mean molecular weight first (eq 3)
        self._write(self.line('Compute mean molecular wt first'))
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        # now compute conversion
        self._write(self.line('Now compute conversion'))
        for species in self.species:
            self._write('y[%d] = x[%d]*%f/XW; ' % (
                species.id, species.id, species.weight) )

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckxtcp(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert x[species] (mole fracs) to c[species] (molar conc)'))
        self._write('void CKXTCP'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * c)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double PORT = (*P)/(%g * (*T)); ' % (R*kelvin*mole/erg) +
                    self.line('P/RT'))
        # now compute conversion
        self._write()
        self._write(self.line('Compute conversion, see Eq 10'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('c[id] = x[id]*PORT;')
        self._outdent()
        self._write('}')

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckxtcr(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert x[species] (mole fracs) to c[species] (molar conc)'))
        self._write('void CKXTCR'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * c)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double XW = 0; '+self.line('See Eq 4, 11 in CK Manual'))
        self._write('double ROW; ')
        
        # compute mean molecular weight first (eq 3)
        self._write(self.line('Compute mean molecular wt first'))
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        # now compute conversion
        self._write('ROW = (*rho) / XW;')
        self._write()
        self._write(self.line('Compute conversion, see Eq 11'))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('c[id] = x[id]*ROW;')
        self._outdent()
        self._write('}')

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckctx(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert c[species] (molar conc) to x[species] (mole fracs)'))
        self._write('void CKCTX'+sym+'(double * c, int * iwrk, double * rwrk, double * x)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))
        self._write('double sumC = 0; ')

        self._write()
        self._write(self.line('compute sum of c '))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('sumC += c[id];')
        self._outdent()
        self._write('}')

        # now compute conversion
        self._write()
        self._write(self.line(' See Eq 13 '))
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('x[id] = c[id]/sumC;')
        self._outdent()
        self._write('}')

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckcty(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert c[species] (molar conc) to y[species] (mass fracs)'))
        self._write('void CKCTY'+sym+'(double * c, int * iwrk, double * rwrk, double * y)')
        self._write('{')
        self._indent()

        self._write('double CW = 0; '+self.line('See Eq 12 in CK Manual'))
        
        # compute denominator in eq 12
        self._write(self.line('compute denominator in eq 12 first'))
        for species in self.species:
            self._write('CW += c[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        # now compute conversion
        self._write(self.line('Now compute conversion'))
        for species in self.species:
            self._write('y[%d] = c[%d]*%f/CW; ' % (
                species.id, species.id, species.weight) )

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
    def _ckcpor(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get Cp/R as a function of T '))
        self._write(self.line('for all species (Eq 19)'))
        self._write('void CKCPOR'+sym+'(double *T, int * iwrk, double * rwrk, double * cpor)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('cp_R(cpor, tc);')
        
        self._outdent()

        self._write('}')

        return
    
    def _ckhort(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get H/RT as a function of T '))
        self._write(self.line('for all species (Eq 20)'))
        self._write('void CKHORT'+sym+'(double *T, int * iwrk, double * rwrk, double * hort)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('speciesEnthalpy(hort, tc);')
        
        self._outdent()

        self._write('}')

        return
 
    def _cksor(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('get S/R as a function of T '))
        self._write(self.line('for all species (Eq 21)'))
        self._write('void CKSOR'+sym+'(double *T, int * iwrk, double * rwrk, double * sor)')
        self._write('{')
        self._indent()

        # get temperature cache
        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        
        # call routine
        self._write('speciesEntropy(sor, tc);')
        
        self._outdent()

        self._write('}')

        return


    def _ckqc(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        self._write()
        self._write()
        self._write(self.line('Returns the rate of progress for each reaction'))
        self._write('void CKQC'+sym+'(double * T, double * C, int * iwrk, double * rwrk, double * qdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        # convert C to SI units
        self._write()
        self._write(self.line('convert to SI'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('C[id] *= 1.0e6;')
        self._outdent()
        self._write('}')
        
        # call productionRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('progressRate(qdot, C, *T);')

        # convert C to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('C[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')

        # convert qdot to chemkin units
        self._write()
        self._write('for (id = 0; id < %d; ++id) {' % nReactions)
        self._indent()
        self._write('qdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return

    
    def _ckkfkr(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())
        
        self._write()
        self._write()
        self._write(self.line('Returns the progress rates of each reactions'))
        self._write(self.line('Given P, T, and mole fractions'))
        self._write('void CKKFKR'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * q_f, double * q_r)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % nSpecies + self.line('temporary storage'))
        
        self._write('double PORT = 1e6 * (*P)/(%g * (*T)); ' % (R*kelvin*mole/erg) +
                    self.line('1e6 * P/RT so c goes to SI units'))
        
        # now compute conversion
        self._write()
        self._write(self.line('Compute conversion, see Eq 10'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('c[id] = x[id]*PORT;')
        self._outdent()
        self._write('}')
        
        # call progressRateFR
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('progressRateFR(q_f, q_r, c, *T);')

        # convert qdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nReactions )
        self._indent()
        self._write('q_f[id] *= 1.0e-6;')
        self._write('q_r[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckqyp(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())
        
        self._write()
        self._write()
        self._write(self.line('Returns the progress rates of each reactions'))
        self._write(self.line('Given P, T, and mass fractions'))
        self._write('void CKQYP'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * qdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % nSpecies + self.line('temporary storage'))
        self._write('double YOW = 0; ')
        self._write('double PWORT; ')
        
        # compute inverse of mean molecular weight first (eq 3)
        self._write(self.line('Compute inverse of mean molecular wt first'))
        for species in self.species:
            self._write('YOW += y[%d]/%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))
 
        self._write(self.line('PW/RT (see Eq. 7)'))
        self._write('PWORT = (*P)/(YOW * %g * (*T)); ' % (R*kelvin*mole/erg) )
        
        self._write(self.line('multiply by 1e6 so c goes to SI'))
        self._write('PWORT *= 1e6; ')

        # now compute conversion
        self._write(self.line('Now compute conversion (and go to SI)'))
        for species in self.species:
            self._write('c[%d] = PWORT * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )

        # call progressRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('progressRate(qdot, c, *T);')

        # convert qdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nReactions )
        self._indent()
        self._write('qdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckqxp(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())
        
        self._write()
        self._write()
        self._write(self.line('Returns the progress rates of each reactions'))
        self._write(self.line('Given P, T, and mole fractions'))
        self._write('void CKQXP'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * qdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % nSpecies + self.line('temporary storage'))
        
        self._write('double PORT = 1e6 * (*P)/(%g * (*T)); ' % (R*kelvin*mole/erg) +
                    self.line('1e6 * P/RT so c goes to SI units'))
        
        # now compute conversion
        self._write()
        self._write(self.line('Compute conversion, see Eq 10'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('c[id] = x[id]*PORT;')
        self._outdent()
        self._write('}')
        
        # call progressRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('progressRate(qdot, c, *T);')

        # convert qdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nReactions )
        self._indent()
        self._write('qdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckqyr(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())
        
        self._write()
        self._write()
        self._write(self.line('Returns the progress rates of each reactions'))
        self._write(self.line('Given rho, T, and mass fractions'))
        self._write('void CKQYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * qdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % nSpecies + self.line('temporary storage'))

        # now compute conversion
        self._write(self.line('See Eq 8 with an extra 1e6 so c goes to SI'))
        for species in self.species:
            self._write('c[%d] = 1e6 * (*rho) * y[%d]/%f; ' % (
                species.id, species.id, species.weight) )
            
        # call progressRate
        self._write()
        self._write(self.line('call progressRate'))
        self._write('progressRate(qdot, c, *T);')

        # convert qdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nReactions )
        self._indent()
        self._write('qdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return


    def _ckqxr(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())
        
        self._write()
        self._write()
        self._write(self.line('Returns the progress rates of each reactions'))
        self._write(self.line('Given rho, T, and mole fractions'))
        self._write('void CKQXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * qdot)')
        self._write('{')
        self._indent()

        self._write('int id; ' + self.line('loop counter'))

        self._write('double c[%d]; ' % nSpecies + self.line('temporary storage'))
        
        self._write('double XW = 0; '+self.line('See Eq 4, 11 in CK Manual'))
        self._write('double ROW; ')
        
        # compute mean molecular weight first (eq 3)
        self._write(self.line('Compute mean molecular wt first'))
        for species in self.species:
            self._write('XW += x[%d]*%f; ' % (
                species.id, species.weight) + self.line('%s' % species.symbol))

        # now compute conversion
        self._write(self.line('Extra 1e6 factor to take c to SI'))
        self._write('ROW = 1e6*(*rho) / XW;')
        self._write()
        self._write(self.line('Compute conversion, see Eq 11'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('c[id] = x[id]*ROW;')
        self._outdent()
        self._write('}')
        
        # call progressRate
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('progressRate(qdot, c, *T);')

        # convert qdot to chemkin units
        self._write()
        self._write(self.line('convert to chemkin units'))
        self._write('for (id = 0; id < %d; ++id) {' % nReactions )
        self._indent()
        self._write('qdot[id] *= 1.0e-6;')
        self._outdent()
        self._write('}')
        
        self._outdent()

        self._write('}')

        return

    
    def __ckeqcontent(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        self._write(
            'double tT = *T; '
            + self.line('temporary temperature'))
        self._write(
            'double tc[] = { log(tT), tT, tT*tT, tT*tT*tT, tT*tT*tT*tT }; '
            + self.line('temperature cache'))
        self._write(
            'double gort[%d]; ' % nSpecies + self.line(' temporary storage'))

        # compute the gibbs free energy
        self._write()
        self._write(self.line('compute the Gibbs free energy'))
        self._write('gibbs(gort, tc);')

        # compute the equilibrium constants
        self._write()
        self._write(self.line('compute the equilibrium constants'))
        self._write('equilibriumConstants(eqcon, gort, tT);')

        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            somepow = 0
            for symbol, coefficient in reaction.reactants:
                somepow = somepow - coefficient

            for symbol, coefficient in reaction.products:
                somepow = somepow + coefficient

            if somepow == 0:
                self._write(self.line(
                    'eqcon[%d] *= %g; ' % (reaction.id-1, (1e-6)**somepow) ) )
                
            else:
                self._write( 'eqcon[%d] *= %g; ' % (reaction.id-1, (1e-6)**somepow) ) 
                    



    def _ckeqc(self, mechanism):

        self._write()
        self._write()
        self._write(self.line('Returns the equil constants for each reaction'))
        self._write('void CKEQC'+sym+'(double * T, double * C, int * iwrk, double * rwrk, double * eqcon)')
        self._write('{')
        self._indent()

        self.__ckeqcontent(mechanism)
                
        self._outdent()

        self._write('}')

        return

    
    def _ckeqyp(self, mechanism):

        import pyre
        periodic = pyre.handbook.periodicTable()

        self._write()
        self._write()
        self._write(self.line('Returns the equil constants for each reaction'))
        self._write(self.line('Given P, T, and mass fractions'))
        self._write('void CKEQYP'+sym+'(double * P, double * T, double * y, int * iwrk, double * rwrk, double * eqcon)')
        self._write('{')
        self._indent()

        self.__ckeqcontent(mechanism)
        
        self._outdent()

        self._write('}')

        return


    def _ckeqxp(self, mechanism):

        import pyre
        periodic = pyre.handbook.periodicTable()

        self._write()
        self._write()
        self._write(self.line('Returns the equil constants for each reaction'))
        self._write(self.line('Given P, T, and mole fractions'))
        self._write('void CKEQXP'+sym+'(double * P, double * T, double * x, int * iwrk, double * rwrk, double * eqcon)')
        self._write('{')
        self._indent()

        self.__ckeqcontent(mechanism)
        
        self._outdent()

        self._write('}')

        return


    def _ckeqyr(self, mechanism):

        import pyre
        periodic = pyre.handbook.periodicTable()

        self._write()
        self._write()
        self._write(self.line('Returns the equil constants for each reaction'))
        self._write(self.line('Given rho, T, and mass fractions'))
        self._write('void CKEQYR'+sym+'(double * rho, double * T, double * y, int * iwrk, double * rwrk, double * eqcon)')
        self._write('{')
        self._indent()

        self.__ckeqcontent(mechanism)
        
        self._outdent()

        self._write('}')

        return


    def _ckeqxr(self, mechanism):

        import pyre
        periodic = pyre.handbook.periodicTable()

        self._write()
        self._write()
        self._write(self.line('Returns the equil constants for each reaction'))
        self._write(self.line('Given rho, T, and mole fractions'))
        self._write('void CKEQXR'+sym+'(double * rho, double * T, double * x, int * iwrk, double * rwrk, double * eqcon)')
        self._write('{')
        self._indent()

        self.__ckeqcontent(mechanism)
        
        self._outdent()

        self._write('}')

        return


# Fuego Extensions. All functions in this section has the fe prefix
# All fuctions in this section uses the standard fuego chemkin functions
    def _ck_eytt(self, mechanism):

        nSpecies = len(mechanism.species())
        lowT,highT,dummy = self._analyzeThermodynamics(mechanism)
        
        self._write()
        self._write()
        self._write(self.line(
            'get temperature given internal energy in mass units and mass fracs'))
        self._write('int feeytt'+fsym+'(double * e, double * y, int * iwrk, double * rwrk, double * t)')
        self._write('{')
        self._indent()

        self._write('const int maxiter = 50;')
        self._write('const double tol  = 0.001;')
        self._write('double ein  = *e;')
        self._write('double tmin = %g; // max lower bound for thermo def' % lowT)
        self._write('double tmax = %g; // min upper bound for thermo def' % highT)
        self._write('double e1,emin,emax,cv,t1,dt;')
        self._write('int i; // loop counter')
        self._write('CKUBMS'+sym+'(&tmin, y, iwrk, rwrk, &emin);')
        self._write('CKUBMS'+sym+'(&tmax, y, iwrk, rwrk, &emax);')
        self._write('if (ein < emin) {')
        self._indent()
        self._write(self.line('Linear Extrapolation below tmin'))
        self._write('CKCVBS'+sym+'(&tmin, y, iwrk, rwrk, &cv);')
        self._write('*t = tmin - (emin-ein)/cv;')
        self._write('return 1;')
        self._outdent()
        self._write('}')
        
        self._write('if (ein > emax) {')
        self._indent()
        self._write(self.line('Linear Extrapolation above tmax'))
        self._write('CKCVBS'+sym+'(&tmax, y, iwrk, rwrk, &cv);')
        self._write('*t = tmax - (emax-ein)/cv;')
        self._write('return 1;')
        self._outdent()
        self._write('}')

        self._write('t1 = tmin + (tmax-tmin)/(emax-emin)*(ein-emin);')
        self._write('for (i = 0; i < maxiter; ++i) {')
        self._indent()
        self._write('CKUBMS'+sym+'(&t1,y,iwrk,rwrk,&e1);')
        self._write('CKCVBS'+sym+'(&t1,y,iwrk,rwrk,&cv);')
        self._write('dt = (ein - e1) / cv;')
        self._write('if (dt > 100) { dt = 100; }')
        self._write('else if (dt < -100) { dt = -100; }')
        self._write('else if (fabs(dt) < tol) break;')
        self._write('t1 += dt;')
        self._outdent()
        self._write('}')
        
        self._write('*t = t1;')
        self._write('return 0;')
        
        self._outdent()

        self._write('}')

        return

 
    def _ck_phity(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert phi[species] (specific mole nums) to y[species] (mass fracs)'))
        self._write('void fephity'+fsym+'(double * phi, int * iwrk, double * rwrk, double * y)')
        self._write('{')
        self._indent()

        self._write('double XW  = 0; ')
        self._write('int id; ' + self.line('loop counter'))
        
        # compute mean molecular weight first (eq 3)
        self._write(self.line('Compute mean molecular wt first'))
        for species in self.species:
            self._write('y[%d] = phi[%d]*%f;   XW += y[%d]; ' % (
                species.id, species.id, species.weight, species.id) +
                        self.line('%s' % species.symbol))
 
        self._write('for (id = 0; id < %d; ++id) {' % self.nSpecies)
        self._indent()
        self._write('y[id] = y[id]/XW;')
        self._outdent()
        self._write('}')
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
 
    def _ck_ytphi(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'convert y[species] (mass fracs) to phi[species] (specific mole num)'))
        self._write('void feytphi'+fsym+'(double * y, int * iwrk, double * rwrk, double * phi)')
        self._write('{')
        self._indent()

        for species in self.species:
            self._write('phi[%d] = y[%d]/%15.8e; ' % (
                species.id, species.id, species.weight/1000.0) +
                        self.line('%s (wt in kg)' % species.symbol))
 
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _ck_ctyr(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'reverse of ytcr, useful for rate computations'))
        self._write('void fectyr'+fsym+'(double * c, double * rho, int * iwrk, double * rwrk, double * y)')
        self._write('{')
        self._indent()

        # now compute conversion
        for species in self.species:
            self._write('y[%d] = c[%d] * %f / (*rho); ' % (
                species.id, species.id, species.weight) )
        
        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 
 
 
    def _ck_cvrhs(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'ddebdf compatible right hand side of CV burner'))
        self._write(self.line(
            'rwrk[0] and rwrk[1] should contain rho and ene respectively'))
        self._write(self.line(
            'working variable phi contains specific mole numbers'))
        self._write('void fecvrhs'+fsym+'(double * time, double * phi, double * phidot, double * rwrk, int * iwrk)')

	self._write('{')
	self._indent()
	# main body
        self._write('double rho,ene; ' + self.line('CV Parameters'))
        self._write('double y[%s], wdot[%s]; ' % (self.nSpecies, self.nSpecies) +
                    self.line('temporary storage'))
        self._write('int i; ' + self.line('Loop counter'))
        self._write('double temperature,pressure; ' + self.line('temporary var'))
        self._write('rho = rwrk[0];')
        self._write('ene = rwrk[1];')
        self._write('fephity'+fsym+'(phi, iwrk, rwrk, y);')
        self._write('feeytt'+fsym+'(&ene, y, iwrk, rwrk, &temperature);')
        self._write('CKPY'+sym+'(&rho, &temperature,  y, iwrk, rwrk, &pressure);')
        self._write('CKWYP'+sym+'(&pressure, &temperature,  y, iwrk, rwrk, wdot);')
        self._write('for (i=0; i<%s; ++i) phidot[i] = wdot[i] / (rho/1000.0); ' % self.nSpecies)
        self._write()
        self._write('return;')

	self._outdent()
	self._write('}')
	return


    def _ck_cvdim(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'returns the dimensionality of the cv burner (number of species)'))
        self._write('int fecvdim'+fsym+'()')

	self._write('{')
	self._indent()
	# main body
        self._write('return %d;' % self.nSpecies)

	self._outdent()
	self._write('}')
	return

 
    def _ck_zndrhs(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'ddebdf compatible right hand side of ZND solver'))
        self._write(self.line( 'rwrk[0] : scaling factor for pressure'))
        self._write(self.line( 'rwrk[1] : preshock density (g/cc) '))
        self._write(self.line( 'rwrk[2] : detonation velocity (cm/s) '))
        self._write(self.line( 'solution vector: [P; rho; y0 ... ylast] '))
        self._write('void fezndrhs'+fsym+'(double * time, double * z, double * zdot, double * rwrk, int * iwrk)')

	self._write('{')
	self._indent()
	# main body
        self._write('double psc,rho1,udet; ' + self.line('ZND Parameters'))
        self._write('double wt[%s], hms[%s], wdot[%s]; ' %
                    (self.nSpecies, self.nSpecies, self.nSpecies) +
                    self.line('temporary storage'))
        self._write('int i; ' + self.line('Loop counter'))
        self._write(self.line('temporary variables'))
        self._write('double ru, T, uvel, wtm, p, rho, gam, son, xm, sum, drdy, eta, cp, cv ;')
        self._write('double *y; ' + self.line('mass frac pointer'))
        self._write()
        self._write('ru = %g;' % (R * mole * kelvin / erg))
        self._write()
        self._write('psc = rwrk[0];')
        self._write('rho1 = rwrk[1];')
        self._write('udet = rwrk[2];')
        self._write()
        self._write('p = z[0] * psc;')
        self._write('rho = z[1];')
        self._write()
        self._write('y = &z[3];')
        self._write()
        self._write('CKMMWY'+sym+'(y, 0, 0, &wtm);')
        self._write()
        self._write('T = p * wtm / rho / ru;')
        self._write()
        self._write('uvel = (rho1 * udet)/ rho;')
        self._write()
        self._write('CKCPBS'+sym+'(&T, y, 0, 0, &cp);')
        self._write('CKCVBS'+sym+'(&T, y, 0, 0, &cv);')
        self._write('gam = cp/cv;')
        self._write()
        self._write('son = sqrt(fabs(gam*ru*T/wtm));')
        self._write('xm = uvel/son;')
        self._write()
        self._write('CKHMS'+sym+'(&T, 0, 0, hms);')
        self._write('CKWT'+sym+'(0, 0, wt);')
        self._write('CKWYP'+sym+'(&p, &T, y, 0, 0, wdot);')
        self._write()
        self._write('sum = 0.0;')
        self._write('for (i=0; i<%s; ++i) {' % self.nSpecies)
        self._indent()
        self._write('zdot[i+3] = wdot[i] * wt[i] / rho;')
        self._write('drdy = -rho * wtm / wt[i];')
        self._write('sum += -( drdy + rho * hms[i]/ (cp*T) ) * zdot[i+3];')
        self._outdent()
        self._write('}')
        self._write()
        self._write('eta = 1.0 - xm*xm;')
        self._write('zdot[0] = -(uvel*uvel/eta/psc)*sum;')
        self._write('zdot[1] = -sum/eta;')
        self._write('zdot[2] = uvel;')
        self._write()
        self._write('return;')

	self._outdent()
	self._write('}')
	return


    def _ck_znddim(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'returns the dimensionality of the ZND solver (3+number of species)'))
        self._write('int feznddim'+fsym+'()')

	self._write('{')
	self._indent()
	# main body
        self._write('return %d;' % (self.nSpecies + 3) )

	self._outdent()
	self._write('}')
	return
    
    def _ck_mechfile(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'returns the name of the source mechanism file '))
        self._write('char* femechfile'+fsym+'()')

	self._write('{')
	self._indent()
	# main body
        self._write('return "%s";' % mechanism.name())

	self._outdent()
	self._write('}')
	return

    def _ck_symnum(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'returns the species number'))
        self._write('int fesymnum'+fsym+'(const char* s1)')

	self._write('{')
	self._indent()
        
        for species in self.species:
            self._write('if (strcmp(s1, "%s")==0) return %d; ' % (
                species.symbol, species.id))
 
        self._write(self.line( 'species name not found' ))
        self._write('return -1;')

	self._outdent()
	self._write('}')
	return
    
    def _ck_symname(self, mechanism):
        self._write()
        self._write()
        self._write(self.line(
            'returns the species name'))
        self._write('char* fesymname'+fsym+'(int sn)')

	self._write('{')
	self._indent()

        for species in self.species:
            self._write('if (sn==%d) return "%s"; ' % (
                species.id, species.symbol))
 
        self._write(self.line( 'species name not found' ))
        self._write('return "NOTFOUND";')

	self._outdent()
	self._write('}')
	return
    
# Fuego's core routines section begins here
    def _molecularWeight(self, mechanism):

        import pyre
        periodic = pyre.handbook.periodicTable()
        
        nSpecies = len(mechanism.species())
        self._write()
        self._write()
        self._write(self.line('save molecular weights into array'))
        self._write('void molecularWeight(double * wt)')
        self._write('{')
        self._indent()

        # molecular weights of all species
        for species in mechanism.species():

            weight = 0.0 #species.molecularWeight()
            for elem, coef in species.composition:
                aw = mechanism.element(elem).weight
                if not aw:
                    aw = periodic.symbol(elem.capitalize()).atomicWeight
                weight += coef * aw

            self._write('wt[%d] = %f; ' % (
                species.id, weight) + self.line('%s' % species.symbol))

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return 

    def _productionRate(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        # OMP stuff
        self._write()
        self._write('static double T_save = -1;')
        self._write('#ifdef BL_USE_OMP')
        self._write('#pragma omp threadprivate(T_save)')
        self._write('#endif')
        self._write()
        self._write('static double k_f_save[%d];' % nReactions)
        self._write('#ifdef BL_USE_OMP')
        self._write('#pragma omp threadprivate(k_f_save)')
        self._write('#endif')
        self._write()
        self._write('static double Kc_save[%d];' % nReactions)
        self._write('#ifdef BL_USE_OMP')
        self._write('#pragma omp threadprivate(Kc_save)')
        self._write('#endif')

        self._write()
        self._write(self.line('compute the production rate for each species'))
        self._write('void productionRate(double * wdot, double * sc, double T)')
        self._write('{')
        self._indent()

        # declarations
        self._write('double qdot;')
        self._initializeRateCalculation(mechanism)

        # clear out wdot
        self._write()
        self._write(self.line('zero out wdot'))
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('wdot[id] = 0.0;')
        self._outdent()
        self._write('}')

        # Save temperature
        self._write()
        self._write('if (T != T_save)')
        self._write('{')
        self._indent()
        self._write('T_save = T;')
        self._write()
        for reaction in mechanism.reaction():
            dim = self._phaseSpaceUnits(reaction.reactants)
            arrhenius = self._arrhenius(reaction, reaction.arrhenius)
            thirdBody = reaction.thirdBody
            low = reaction.low
            if not thirdBody:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            elif not low:
                uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
            else:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            self._write("k_f_save[%d] = %g * %s;" % (reaction.id-1,uc.value, arrhenius))
        self._write()
        for reaction in mechanism.reaction():
            K_c = self._Kc(mechanism, reaction)
            self._write("Kc_save[%d] = %s;" % (reaction.id-1,K_c))

        self._outdent()
        self._write('}')        
        
        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            # compute the rates
            self._forwardRate(mechanism, reaction)
            self._reverseRate(mechanism, reaction)

            # store the progress rate
            self._write("qdot = q_f - q_r;")

            for symbol, coefficient in reaction.reactants:
                self._write(
                    "wdot[%d] -= %d * qdot;" % (mechanism.species(symbol).id, coefficient))

            for symbol, coefficient in reaction.products:
                self._write(
                    "wdot[%d] += %d * qdot;" % (mechanism.species(symbol).id, coefficient))

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _progressRate(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        self._write()
        self._write()
        self._write(self.line('compute the progress rate for each reaction'))
        self._write('void progressRate(double * qdot, double * sc, double T)')
        self._write('{')
        self._indent()

        # declarations
        self._initializeRateCalculation(mechanism)
        
        # Save temperature
        self._write()
        self._write('if (T != T_save)')
        self._write('{')
        self._indent()
        self._write('T_save = T;')
        self._write()
        for reaction in mechanism.reaction():
            dim = self._phaseSpaceUnits(reaction.reactants)
            arrhenius = self._arrhenius(reaction, reaction.arrhenius)
            thirdBody = reaction.thirdBody
            low = reaction.low
            if not thirdBody:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            elif not low:
                uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
            else:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            self._write("k_f_save[%d] = %g * %s;" % (reaction.id-1,uc.value, arrhenius))
        self._write()
        for reaction in mechanism.reaction():
            K_c = self._Kc(mechanism, reaction)
            self._write("Kc_save[%d] = %s;" % (reaction.id-1,K_c))

        self._outdent()
        self._write('}')        

        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            # compute the rates
            self._forwardRate(mechanism, reaction)
            self._reverseRate(mechanism, reaction)

            # store the progress rate
            self._write("qdot[%d] = q_f - q_r;" % (reaction.id - 1))

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _initializeRateCalculation(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        # declarations
        self._write()
        self._write('int id; ' + self.line('loop counter'))

        self._write('double mixture;                 '
                    + self.line('mixture concentration'))
        self._write('double g_RT[%d];                ' % nSpecies
                    + self.line('Gibbs free energy'))

        self._write('double Kc;                      ' + self.line('equilibrium constant'))
        self._write('double k_f;                     ' + self.line('forward reaction rate'))
        self._write('double k_r;                     ' + self.line('reverse reaction rate'))
        self._write('double q_f;                     ' + self.line('forward progress rate'))
        self._write('double q_r;                     ' + self.line('reverse progress rate'))
        self._write('double phi_f;                   '
                    + self.line('forward phase space factor'))
        self._write('double phi_r;                   '
                    + self.line('reverse phase space factor'))
        self._write('double alpha;                   ' + self.line('enhancement'))


        self._write('double redP;                    ' + self.line('reduced pressure'))
        self._write('double logPred;                 ' + self.line('log of above'))
        self._write('double F;                       '
                    + self.line('fallof rate enhancement'))
        self._write()
        self._write('double F_troe;                  ' + self.line('TROE intermediate'))
        self._write('double logFcent;                ' + self.line('TROE intermediate'))
        self._write('double troe;                    ' + self.line('TROE intermediate'))
        self._write('double troe_c;                  ' + self.line('TROE intermediate'))
        self._write('double troe_n;                  ' + self.line('TROE intermediate'))

        self._write()
        self._write('double X;                       ' + self.line('SRI intermediate'))
        self._write('double F_sri;                   ' + self.line('SRI intermediate'))

        self._write(
            'double tc[] = { log(T), T, T*T, T*T*T, T*T*T*T }; '
            + self.line('temperature cache'))

        self._write()
        self._write('double invT = 1.0 / tc[1];')

        # compute the reference concentration
        self._write()
        self._write(self.line('reference concentration: P_atm / (RT) in inverse mol/m^3'))
        self._write('double refC = %g / %g / T;' % (atm.value, R.value))

        # compute the mixture concentration
        self._write()
        self._write(self.line('compute the mixture concentration'))
        self._write('mixture = 0.0;')
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('mixture += sc[id];')
        self._outdent()
        self._write('}')

        # compute the Gibbs free energies
        self._write()
        self._write(self.line('compute the Gibbs free energy'))
        self._write('gibbs(g_RT, tc);')
        
        return


    def _forwardRate(self, mechanism, reaction):

        lt = reaction.lt
        if lt:
            import pyre
            pyre.debug.Firewall.hit("Landau-Teller reactions are not supported yet")
            return self._landau(reaction)

        dim = self._phaseSpaceUnits(reaction.reactants)

        phi_f = self._phaseSpace(mechanism, reaction.reactants)
        self._write("phi_f = %s;" % phi_f)

        arrhenius = self._arrhenius(reaction, reaction.arrhenius)

        thirdBody = reaction.thirdBody
        if not thirdBody:
            uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            self._write("k_f = k_f_save[%d];" % (reaction.id-1))
            self._write("q_f = phi_f * k_f;")
            return
            
        alpha = self._enhancement(mechanism, reaction)
        self._write("alpha = %s;" % alpha)

        sri = reaction.sri
        low = reaction.low
        troe = reaction.troe

        if not low:
            uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
            self._write("k_f = alpha * k_f_save[%d];" % (reaction.id-1))
            self._write("q_f = phi_f * k_f;")
            return

        uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
        self._write("k_f = k_f_save[%d];" % (reaction.id-1))
        k_0 = self._arrhenius(reaction, reaction.low)
        redP = "alpha / k_f * " + k_0
        self._write("redP = 1e-%d * %s;" % (dim*6,redP))
        self._write("F = redP / (1 + redP);")

        if sri:
            self._write("logPred = log10(redP);")

            self._write("X = 1.0 / (1.0 + logPred*logPred);")

            divisor=-sri[2]
            if abs(divisor) > 1e-100:
                SRI = "exp(X * log(%g*exp(%g/T) + exp(T/%g))" % (sri[0], -sri[1], divisor)
            else:
                SRI = "exp(X * log(%g*exp(%g/T))" % (sri[0], -sri[1])
            if len(sri) > 3:
                SRI += " * %g * exp(%g*tc[0])" % (sri[3], sri[4])

            self._write("F_sri = %s;" % SRI)
            self._write("F *= Ftroe;")

        elif troe:
            self._write("logPred = log10(redP);")

            logF_cent = "logFcent = log10("
            if abs(troe[1]) > 1e-100:
                logF_cent += "(%g*exp(T/%g))" % (1-troe[0], -troe[1])
            if abs(troe[2]) > 1e-100:
                logF_cent += "+ (%g*exp(T/%g))" % (troe[0], -troe[2])
            if len(troe) == 4:
                logF_cent += "+ (exp(%g/T))" % (-troe[3])
            logF_cent += ');'
            self._write(logF_cent)
            
            d = .14
            self._write("troe_c = -.4 - .67 * logFcent;")
            self._write("troe_n = .75 - 1.27 * logFcent;")
            self._write("troe = (troe_c + logPred) / (troe_n - .14*(troe_c + logPred));")
            self._write("F_troe = pow(10, logFcent / (1.0 + troe*troe));")
            self._write("F *= F_troe;")

        self._write("k_f *= F;")
        self._write("q_f = phi_f * k_f;")
        return
        

    def _reverseRate(self, mechanism, reaction):
        if not reaction.reversible:
            self._write("q_r = 0.0;")
            return

        phi_r = self._phaseSpace(mechanism, reaction.products)
        self._write("phi_r = %s;" % phi_r)

        if reaction.rlt:
            import pyre
            pyre.debug.Firewall.hit("Landau-Teller reactions are not supported yet")
            return

        if reaction.rev:
            dim = self._phaseSpaceUnits(reaction.reactants)
            arrhenius = self._arrhenius(reaction, reaction.rev)
            thirdBody = reaction.thirdBody
            if thirdBody:
                uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
                self._write("k_r = %g * alpha * %s;" % (uc.value, arrhenius))
            else:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
                self._write("k_r = %g * %s;" % (uc.value, arrhenius))

            self._write("q_f = phi_r * k_r;")
            return
        
        self._write("Kc = Kc_save[%d];" % (reaction.id-1))

        self._write("k_r = k_f / Kc;")
        self._write("q_r = phi_r * k_r;")

        return


    def _progressRateFR(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        self._write()
        self._write()
        self._write(self.line('compute the progress rate for each reaction'))
        self._write('void progressRateFR(double * q_f, double * q_r, double * sc, double T)')
        self._write('{')
        self._indent()

        # declarations
        self._initializeRateCalculationFR(mechanism)
        
        # Save temperature
        self._write()
        self._write('if (T != T_save)')
        self._write('{')
        self._indent()
        self._write('T_save = T;')
        self._write()
        for reaction in mechanism.reaction():
            dim = self._phaseSpaceUnits(reaction.reactants)
            arrhenius = self._arrhenius(reaction, reaction.arrhenius)
            thirdBody = reaction.thirdBody
            low = reaction.low
            if not thirdBody:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            elif not low:
                uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
            else:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            self._write("k_f_save[%d] = %g * %s;" % (reaction.id-1,uc.value, arrhenius))
        self._write()
        for reaction in mechanism.reaction():
            K_c = self._Kc(mechanism, reaction)
            self._write("Kc_save[%d] = %s;" % (reaction.id-1,K_c))

        self._outdent()
        self._write('}')        

        for reaction in mechanism.reaction():

            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            # compute the rates
            self._forwardRateFR(mechanism, reaction)
            self._reverseRateFR(mechanism, reaction)

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _initializeRateCalculationFR(self, mechanism):

        nSpecies = len(mechanism.species())
        nReactions = len(mechanism.reaction())

        # declarations
        self._write()
        self._write('int id; ' + self.line('loop counter'))

        self._write('double mixture;                 '
                    + self.line('mixture concentration'))
        self._write('double g_RT[%d];                ' % nSpecies
                    + self.line('Gibbs free energy'))

        self._write('double Kc;                      ' + self.line('equilibrium constant'))
        self._write('double k_f;                     ' + self.line('forward reaction rate'))
        self._write('double k_r;                     ' + self.line('reverse reaction rate'))
        self._write('double phi_f;                   '
                    + self.line('forward phase space factor'))
        self._write('double phi_r;                   '
                    + self.line('reverse phase space factor'))
        self._write('double alpha;                   ' + self.line('enhancement'))


        self._write('double redP;                    ' + self.line('reduced pressure'))
        self._write('double logPred;                 ' + self.line('log of above'))
        self._write('double F;                       '
                    + self.line('fallof rate enhancement'))
        self._write()
        self._write('double F_troe;                  ' + self.line('TROE intermediate'))
        self._write('double logFcent;                ' + self.line('TROE intermediate'))
        self._write('double troe;                    ' + self.line('TROE intermediate'))
        self._write('double troe_c;                  ' + self.line('TROE intermediate'))
        self._write('double troe_n;                  ' + self.line('TROE intermediate'))

        self._write()
        self._write('double X;                       ' + self.line('SRI intermediate'))
        self._write('double F_sri;                   ' + self.line('SRI intermediate'))

        self._write(
            'double tc[] = { log(T), T, T*T, T*T*T, T*T*T*T }; '
            + self.line('temperature cache'))

        self._write()
        self._write('double invT = 1.0 / tc[1];')

        # compute the reference concentration
        self._write()
        self._write(self.line('reference concentration: P_atm / (RT) in inverse mol/m^3'))
        self._write('double refC = %g / %g / T;' % (atm.value, R.value))

        # compute the mixture concentration
        self._write()
        self._write(self.line('compute the mixture concentration'))
        self._write('mixture = 0.0;')
        self._write('for (id = 0; id < %d; ++id) {' % nSpecies)
        self._indent()
        self._write('mixture += sc[id];')
        self._outdent()
        self._write('}')

        # compute the Gibbs free energies
        self._write()
        self._write(self.line('compute the Gibbs free energy'))
        self._write('gibbs(g_RT, tc);')
        
        return


    def _forwardRateFR(self, mechanism, reaction):

        lt = reaction.lt
        if lt:
            import pyre
            pyre.debug.Firewall.hit("Landau-Teller reactions are not supported yet")
            return self._landau(reaction)

        dim = self._phaseSpaceUnits(reaction.reactants)

        phi_f = self._phaseSpace(mechanism, reaction.reactants)
        self._write("phi_f = %s;" % phi_f)

        arrhenius = self._arrhenius(reaction, reaction.arrhenius)

        thirdBody = reaction.thirdBody
        if not thirdBody:
            uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
            self._write("k_f = k_f_save[%d];" % (reaction.id-1))
            self._write("q_f[%d] = phi_f * k_f;" % (reaction.id - 1))
            return
            
        alpha = self._enhancement(mechanism, reaction)
        self._write("alpha = %s;" % alpha)

        sri = reaction.sri
        low = reaction.low
        troe = reaction.troe

        if not low:
            uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
            self._write("k_f = alpha * k_f_save[%d];" % (reaction.id-1))
            self._write("q_f[%d] = phi_f * k_f;" % (reaction.id - 1))
            return

        uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
        self._write("k_f = k_f_save[%d];" % (reaction.id-1))
        k_0 = self._arrhenius(reaction, reaction.low)
        redP = "alpha / k_f * " + k_0
        self._write("redP = 1e-%d * %s;" % (dim*6,redP))
        self._write("F = redP / (1 + redP);")

        if sri:
            self._write("logPred = log10(redP);")

            self._write("X = 1.0 / (1.0 + logPred*logPred);")

            divisor=-sri[2]
            if abs(divisor) > 1e-100:
                SRI = "exp(X * log(%g*exp(%g/T) + exp(T/%g))" % (sri[0], -sri[1], divisor)
            else:
                SRI = "exp(X * log(%g*exp(%g/T))" % (sri[0], -sri[1])
            if len(sri) > 3:
                SRI += " * %g * exp(%g*tc[0])" % (sri[3], sri[4])

            self._write("F_sri = %s;" % SRI)
            self._write("F *= Ftroe;")

        elif troe:
            self._write("logPred = log10(redP);")

            logF_cent = "logFcent = log10("
            if abs(troe[1]) > 1e-100:
                logF_cent += "(%g*exp(T/%g))" % (1-troe[0], -troe[1])
            if abs(troe[2]) > 1e-100:
                logF_cent += "+ (%g*exp(T/%g))" % (troe[0], -troe[2])
            if len(troe) == 4:
                logF_cent += "+ (exp(%g/T))" % (-troe[3])
            logF_cent += ');'
            self._write(logF_cent)
            
            d = .14
            self._write("troe_c = -.4 - .67 * logFcent;")
            self._write("troe_n = .75 - 1.27 * logFcent;")
            self._write("troe = (troe_c + logPred) / (troe_n - .14*(troe_c + logPred));")
            self._write("F_troe = pow(10, logFcent / (1.0 + troe*troe));")
            self._write("F *= F_troe;")

        self._write("k_f *= F;")
        self._write("q_f[%d] = phi_f * k_f;" % (reaction.id - 1))
        return
        

    def _reverseRateFR(self, mechanism, reaction):
        if not reaction.reversible:
            self._write("q_r[%d] = 0.0;" % (reaction.id - 1))
            return

        phi_r = self._phaseSpace(mechanism, reaction.products)
        self._write("phi_r = %s;" % phi_r)

        if reaction.rlt:
            import pyre
            pyre.debug.Firewall.hit("Landau-Teller reactions are not supported yet")
            return

        if reaction.rev:
            dim = self._phaseSpaceUnits(reaction.reactants)
            arrhenius = self._arrhenius(reaction, reaction.rev)
            thirdBody = reaction.thirdBody
            if thirdBody:
                uc = self._prefactorUnits(reaction.units["prefactor"], -dim)
                self._write("k_r = %g * alpha * %s;" % (uc.value, arrhenius))
            else:
                uc = self._prefactorUnits(reaction.units["prefactor"], 1-dim)
                self._write("k_r = %g * %s;" % (uc.value, arrhenius))

            self._write("q_f[%d] = phi_r * k_r;" % (reaction.id - 1))
            return
        
        self._write("Kc = Kc_save[%d];" % (reaction.id-1))

        self._write("k_r = k_f / Kc;")
        self._write("q_r[%d] = phi_r * k_r;" % (reaction.id - 1))

        return


    def _arrhenius(self, reaction, parameters):
        A, beta, E = parameters
        if A == 0:
            return "0.0"
        expr = "%g" % A
        if beta == 0 and E == 0:
            return expr
        expr +="*exp("
        if beta != 0:
            expr += "%g*tc[0]" % beta
        if E != 0:
            uc = self._activationEnergyUnits(reaction.units["activation"])
            expr += "%+.20g*invT" % (- uc * E / Rc / kelvin) # catch unit conversion errors!
        expr += ')'
        
        return expr


    def _prefactorUnits(self, code, exponent):

        if code == "mole/cm**3":
            units = mole / cm**3
        elif code == "moles":
            units = mole / cm**3
        elif code == "molecules":
            import pyre
            units = 1.0 / avogadro / cm**3
        else:
            import pyre
            pyre.debug.Firewall.hit("unknown prefactor units '%s'" % code)
            return 1

        return units ** exponent / second


    def _activationEnergyUnits(self, code):
        if code == "cal/mole":
            units = cal / mole
        elif code == "kcal/mole":
            units = kcal /mole
        elif code == "joules/mole":
            units = J / mole
        elif code == "kjoules/mole":
            units = kJ / mole
        elif code == "kelvins":
            units = Rc * kelvin
        else:
            pyre.debug.Firewall.hit("unknown activation energy units '%s'" % code)
            return 1

        return units


    def _equilibriumConstants(self, mechanism):
        self._write()
        self._write()
        self._write(self.line('compute the equilibrium constants for each reaction'))
        self._write('void equilibriumConstants(double *kc, double * g_RT, double T)')
        self._write('{')
        self._indent()

        # compute the reference concentration
        self._write(self.line('reference concentration: P_atm / (RT) in inverse mol/m^3'))
        self._write('double refC = %g / %g / T;' % (atm.value, R.value))

        # compute the equilibrium constants
        for reaction in mechanism.reaction():
            self._write()
            self._write(self.line('reaction %d: %s' % (reaction.id, reaction.equation())))

            K_c = self._Kc(mechanism, reaction)
            self._write("kc[%d] = %s;" % (reaction.id - 1, K_c))

        self._write()
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _phaseSpace(self, mechanism, reagents):

        phi = []

        for symbol, coefficient in reagents:
            conc = "sc[%d]" % mechanism.species(symbol).id
            phi += [conc] * coefficient

        return "*".join(phi)


    def _phaseSpaceUnits(self, reagents):
        dim = 0
        for symbol, coefficient in reagents:
            dim += coefficient

        return dim


    def _enhancement(self, mechanism, reaction):
        thirdBody = reaction.thirdBody
        if not thirdBody:
            import pyre
            pyre.debug.Firewall.hit("_enhancement called for a reaction without a third body")
            return

        species, coefficient = thirdBody
        efficiencies = reaction.efficiencies

        if not efficiencies:
            if species == "<mixture>":
                return "mixture"
            return "sc[%d]" % mechanism.species(species).id

        alpha = ["mixture"]
        for symbol, efficiency in efficiencies:
            factor = efficiency - 1
            conc = "sc[%d]" % mechanism.species(symbol).id
            if factor == 1:
                alpha.append(conc)
            else:
                alpha.append("%g*%s" % (factor, conc))

        return " + ".join(alpha)


    def _cv(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute Cv/R at the given temperature'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("cv_R", self._cvNASA, speciesInfo)

        return
    
    def _cp(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute Cp/R at the given temperature'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("cp_R", self._cpNASA, speciesInfo)

        return


    def _gibbs(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute the g/(RT) at the given temperature'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("gibbs", self._gibbsNASA, speciesInfo)

        return

    def _helmholtz(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute the a/(RT) at the given temperature'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("helmholtz", self._helmholtzNASA, speciesInfo)

        return

    def _speciesEntropy(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute the S/R at the given temperature (Eq 21)'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("speciesEntropy", self._entropyNASA, speciesInfo)

        return

    def _speciesInternalEnergy(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute the e/(RT) at the given temperature'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("speciesInternalEnergy", self._internalEnergy, speciesInfo)

        return

    def _speciesEnthalpy(self, speciesInfo):

        self._write()
        self._write()
        self._write(self.line('compute the h/(RT) at the given temperature (Eq 20)'))
        self._write(self.line('tc contains precomputed powers of T, tc[0] = log(T)'))
        self._generateThermoRoutine("speciesEnthalpy", self._enthalpyNASA, speciesInfo)

        return

    

    def _generateThermoRoutine(self, name, expressionGenerator, speciesInfo):

        lowT, highT, midpoints = speciesInfo
        
        self._write('void %s(double * species, double * tc)' % name)
        self._write('{')

        self._indent()

        # declarations
        self._write()
        self._write(self.line('temperature'))
        self._write('double T = tc[1], invT = 1.0 / T;')

        # temperature check
        # self._write()
        # self._write(self.line('check the temperature value'))
        # self._write('if (T < %g || T > %g) {' % (lowT, highT))
        # self._indent()
        # self._write(
        #     'fprintf(stderr, "temperature %%g is outside the range (%g, %g)", T);'
        #     % (lowT, highT))
        # self._write('return;')
        # self._outdent()
        # self._write('}')
                    
        for midT, speciesList in midpoints.items():

            self._write('')
            self._write(self.line('species with midpoint at T=%g kelvin' % midT))
            self._write('if (T < %g) {' % midT)
            self._indent()

            for species, lowRange, highRange in speciesList:
                self._write(self.line('species %d: %s' % (species.id, species.symbol)))
                self._write('species[%d] =' % species.id)
                self._indent()
                expressionGenerator(lowRange.parameters)
                self._outdent()

            self._outdent()
            self._write('} else {')
            self._indent()

            for species, lowRange, highRange in speciesList:
                self._write(self.line('species %d: %s' % (species.id, species.symbol)))
                self._write('species[%d] =' % species.id)
                self._indent()
                expressionGenerator(highRange.parameters)
                self._outdent()

            self._outdent()
            self._write('}')
            
        self._write('return;')
        self._outdent()

        self._write('}')

        return


    def _analyzeThermodynamics(self, mechanism):
        lowT = 0.0
        highT = 1000000.0

        midpoints = {}

        for species in mechanism.species():

            models = species.thermo
            if len(models) > 2:
                import pyre
                pyre.debug.Firewall.hit("unsupported configuration in species.thermo")
                return
            
            m1 = models[0]
            m2 = models[1]

            if m1.lowT < m2.lowT:
                lowRange = m1
                highRange = m2
            else:
                lowRange = m2
                highRange = m1

            low = lowRange.lowT
            mid = lowRange.highT
            high = highRange.highT

            if low > lowT:
                lowT = low
            if high < highT:
                highT = high

            midpoints.setdefault(mid, []).append((species, lowRange, highRange))
            
        return lowT, highT, midpoints


    def _Kc(self, mechanism, reaction):

        dim = 0
        dG = ""

        terms = []
        for symbol, coefficient in reaction.reactants:
            if coefficient == 1:
                factor = ""
            else:
                factor = "%d * " % coefficient
                    
            terms.append("%sg_RT[%d]" % (factor, mechanism.species(symbol).id))
            dim -= coefficient
        dG += '(' + ' + '.join(terms) + ')'

        # flip the signs
        terms = []
        for symbol, coefficient in reaction.products:
            if coefficient == 1:
                factor = ""
            else:
                factor = "%d * " % coefficient
            terms.append("%sg_RT[%d]" % (factor, mechanism.species(symbol).id))
            dim += coefficient
        dG += ' - (' + ' + '.join(terms) + ')'

        K_p = 'exp(' + dG + ')'

        if dim == 0:
            conversion = ""
        elif dim > 0:
            conversion = "*".join(["refC"] * dim) + ' * '
        else:
            conversion = "1.0 / (" + "*".join(["refC"] * abs(dim)) + ') * '

        K_c = conversion + K_p

        return K_c


    def _cpNASA(self, parameters):
        self._write('%+15.8e' % parameters[0])
        self._write('%+15.8e * tc[1]' % parameters[1])
        self._write('%+15.8e * tc[2]' % parameters[2])
        self._write('%+15.8e * tc[3]' % parameters[3])
        self._write('%+15.8e * tc[4];' % parameters[4])
        return


    def _cvNASA(self, parameters):
        self._write('%+15.8e' % (parameters[0] - 1.0))
        self._write('%+15.8e * tc[1]' % parameters[1])
        self._write('%+15.8e * tc[2]' % parameters[2])
        self._write('%+15.8e * tc[3]' % parameters[3])
        self._write('%+15.8e * tc[4];' % parameters[4])
        return


    def _enthalpyNASA(self, parameters):
        self._write('%+15.8e' % parameters[0])
        self._write('%+15.8e * tc[1]' % (parameters[1]/2))
        self._write('%+15.8e * tc[2]' % (parameters[2]/3))
        self._write('%+15.8e * tc[3]' % (parameters[3]/4))
        self._write('%+15.8e * tc[4]' % (parameters[4]/5))
        self._write('%+15.8e * invT;' % (parameters[5]))
        return


    def _internalEnergy(self, parameters):
        self._write('%+15.8e' % (parameters[0] - 1.0))
        self._write('%+15.8e * tc[1]' % (parameters[1]/2))
        self._write('%+15.8e * tc[2]' % (parameters[2]/3))
        self._write('%+15.8e * tc[3]' % (parameters[3]/4))
        self._write('%+15.8e * tc[4]' % (parameters[4]/5))
        self._write('%+15.8e * invT;' % (parameters[5]))
        return

    
    def _gibbsNASA(self, parameters):
        self._write('%+20.15e * invT' % parameters[5])
        self._write('%+20.15e' % (parameters[0] - parameters[6]))
        self._write('%+20.15e * tc[0]' % (-parameters[0]))
        self._write('%+20.15e * tc[1]' % (-parameters[1]/2))
        self._write('%+20.15e * tc[2]' % (-parameters[2]/6))
        self._write('%+20.15e * tc[3]' % (-parameters[3]/12))
        self._write('%+20.15e * tc[4];' % (-parameters[4]/20))
        return
    
    def _helmholtzNASA(self, parameters):
        self._write('%+15.8e * invT' % parameters[5])
        self._write('%+15.8e' % (parameters[0] - parameters[6] - 1.0))
        self._write('%+15.8e * tc[0]' % (-parameters[0]))
        self._write('%+15.8e * tc[1]' % (-parameters[1]/2))
        self._write('%+15.8e * tc[2]' % (-parameters[2]/6))
        self._write('%+15.8e * tc[3]' % (-parameters[3]/12))
        self._write('%+15.8e * tc[4];' % (-parameters[4]/20))
        return

    def _entropyNASA(self, parameters):
        self._write('%+15.8e * tc[0]' % parameters[0])
        self._write('%+15.8e * tc[1]' % (parameters[1]))
        self._write('%+15.8e * tc[2]' % (parameters[2]/2))
        self._write('%+15.8e * tc[3]' % (parameters[3]/3))
        self._write('%+15.8e * tc[4]' % (parameters[4]/4))
        self._write('%+15.8e ;' % (parameters[6]))
        return

# version
__id__ = "$Id$"

#  End of file 
