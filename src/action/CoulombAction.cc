#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "CoulombAction.h"
#include "action/coulomb/Coulomb1DLinkAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "advancer/DisplaceMoveSampler.h"
#include "Beads.h"
#include "Paths.h"
#include "util/SuperCell.h"
#include "SimulationInfo.h"
#include "PairAction.h"
#include "ImagePairAction.h"
#include "EwaldImagePairAction.h"
#include "util/TradEwaldSum.h"
#include "util/OptEwaldSum.h"

CoulombAction::CoulombAction(const double epsilon, 
  const SimulationInfo& simInfo, const int norder, double rmin, double rmax,
  int ngpts, const bool dumpFiles, bool useEwald, int ewaldNDim, 
  double ewaldRcut, double ewaldKcut, double screenDist, const double kappa, 
  const int nImages, const std::string ewaldType, int exLevel)
  : epsilon(epsilon), tau(simInfo.getTau()), npart(simInfo.getNPart()),
    pairActionArray(0), screenDist(screenDist), ewaldSum(0) {
  typedef blitz::TinyVector<int, NDIM> IVec;
  const int nspecies=simInfo.getNSpecies();
  if (useEwald && (NDIM==3||NDIM==2) && ewaldNDim==NDIM) {
    SuperCell &cell(*simInfo.getSuperCell());
    if (ewaldRcut==0.) ewaldRcut = cell.a[0]/2.;
    std::cout << "EwaldRcut = " << ewaldRcut << std::endl;
  
    if (ewaldType=="tradEwald") ewaldSum = new TradEwaldSum(cell,npart,ewaldRcut,ewaldKcut, kappa);
    if (ewaldType=="optEwald")  ewaldSum = new OptEwaldSum(cell,npart,ewaldRcut,ewaldKcut,4*ewaldKcut,8);
    rewald.resize(npart);
    EwaldSum::Array &q=ewaldSum->getQArray();  
    for (int i=0; i<npart; ++i) q(i)=simInfo.getPartSpecies(i).charge;
    ewaldSum->evalSelfEnergy();
  }
  for (int i=0; i<nspecies; ++i) {
    for (int j=i; j<nspecies; ++j) {
      const Species &s1=simInfo.getSpecies(i);
      const Species &s2=simInfo.getSpecies(j);
      q1q2=s1.charge*s2.charge/epsilon;
      if (q1q2!=0 && ((s1.name!=s2.name) || s1.count>1) ) {
        mu=1./(1./s1.mass+1./s2.mass);
        displace2 = s1.displace-s2.displace; displace2 *= displace2;
        /// Hack to handle ion-ion interaction properly.
        if (rmin==0) rmin=0.005/(2*((mu>500)?1:mu)*fabs(q1q2)); 
        IVec nimage=0; bool needImages=false;
        if (useEwald && ewaldNDim<NDIM) {
          SuperCell::Vec a=simInfo.getSuperCell()->a;
          for (int idim=0; idim<ewaldNDim; ++idim) {
            nimage[idim]=(int)(rmax/a[idim]);
            if (nimage[idim]>1) nimage[idim]-=2;
            if (nimage[idim]>0) needImages=true;
          }
        } else {
          if (rmax==0) {
            SuperCell::Vec a=simInfo.getSuperCell()->a;
            rmax=sqrt(dot(a,a));
          }
        }
        if (ngpts==0) ngpts=500;
        if (needImages) {
          pairActionArray.push_back(
            // Hack to handle ion-ion interaction properly.
            new ImagePairAction(s1,s2, *this, simInfo, (mu>500)?0:norder,
                  nimage, rmin, rmax, ngpts,(i==j)?exLevel-1:-1));
        } else {
	  if (nImages > 1 && ewaldType=="tradEwald"){
	    pairActionArray.push_back(
              new EwaldImagePairAction(s1,s2, *this, simInfo, (mu>500)?0:norder,
                    rmin, rmax, ngpts, nImages, (i==j)?exLevel-1:-1));
	  } else {
	    pairActionArray.push_back(
              // Hack to handle ion-ion interaction properly.
            new PairAction(s1,s2, *this, simInfo, (mu>500)?0:norder,
                           rmin, rmax, ngpts, false, (i==j)?exLevel-1:-1));
	  }
        }
        if (dumpFiles) (*(pairActionArray.end()-1))->write("",0);
      }
    }
  }
}

CoulombAction::~CoulombAction() {
  for (unsigned int i=0; i<pairActionArray.size(); ++i) {
     delete pairActionArray[i];
  }
  delete ewaldSum;
}

double CoulombAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  double u=0;
  for (unsigned int i=0; i<pairActionArray.size(); ++i) {
    u += pairActionArray[i]->getActionDifference(sampler,level);
  }
  // Compute long range Ewald action at lowest level.
  if (ewaldSum && level==0) {
    const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
    const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
    const int nSlice=sectionBeads.getNSlice();
    const IArray& index=sampler.getMovingIndex(); 
    const int nMoving=index.size();
    for (int islice=1; islice<nSlice-1; ++islice) {
      for (int i=0; i<npart; ++i) rewald(i)=sectionBeads(i,islice); 
      u -= ewaldSum->evalLongRange(rewald)*tau/epsilon; 
      for (int i=0; i<nMoving; ++i) rewald(index(i))=movingBeads(i,islice);
      u += ewaldSum->evalLongRange(rewald)*tau/epsilon; 
    }
  } 
  return u;
}

double CoulombAction::getActionDifference(const Paths &paths, 
    const VArray &displacement, int nmoving, const IArray &movingIndex, 
    int iFirstSlice, int iLastSlice) {

  SuperCell cell=paths.getSuperCell();
  double u=0;
  for (unsigned int i=0; i<pairActionArray.size(); ++i) {
    u += pairActionArray[i]->getActionDifference(paths, displacement, nmoving,
                               movingIndex, iFirstSlice, iLastSlice); 
  }
  // Compute long range Ewald action at lowest level.
  if (ewaldSum) {
     for (int islice=iFirstSlice; islice<=iLastSlice; ++islice) {
      for (int i=0; i<npart; ++i)  rewald(i)=paths(i,islice);
      u -= ewaldSum->evalLongRange(rewald)*tau/epsilon;
      for (int i=0; i<nmoving; ++i) {
        rewald(movingIndex(i))+=displacement(i);
        rewald(movingIndex(i))=cell.pbc(rewald(movingIndex(i)));
      }
      u += ewaldSum->evalLongRange(rewald)*tau/epsilon;
    }
  }
  return u;
}



double CoulombAction::getTotalAction(const Paths& paths, int level) const {
  return 0;
}

void CoulombAction::getBeadAction(const Paths& paths, int ipart, int islice,
         double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const {
  u=utau=ulambda=0; fm=0.; fp=0.;
  for (unsigned int i=0; i<pairActionArray.size(); ++i) {
    double ui=0, utaui=0, ulambdai=0; 
    Vec fmi=0.0, fpi=0.0; 
    pairActionArray[i]->
      getBeadAction(paths,ipart,islice,ui,utaui,ulambdai,fmi,fpi);
    u+=ui; utau+=utaui; ulambda+=ulambdai; fm+=fmi; fp+=fpi;
  }
  // Compute long range action.
  if (ewaldSum && ipart==0) {
    paths.getSlice(islice,rewald);
    double longRange = ewaldSum->evalLongRange(rewald)/epsilon;
    u += longRange*tau;
    utau += longRange;
  }
  //std :: cout << "CA :: "<<ipart<<" "<<islice<<"  "<<utau<<"  "<<u<<std ::endl;
}

double CoulombAction::getAction(const Paths& paths, int islice) const {
  double u=0;
  for (int ipart=0; ipart<npart; ++ipart) {
    for (unsigned int i=0; i<pairActionArray.size(); ++i) {
      double ui=0, utaui=0, ulambdai=0; 
      Vec fmi=0.0, fpi=0.0; 
      pairActionArray[i]->
        getBeadAction(paths,ipart,islice,ui,utaui,ulambdai,fmi,fpi);
      u+=ui;
    }
  }
  // Compute long range action.
  if (ewaldSum) {
    paths.getSlice(islice,rewald);
    double longRange = ewaldSum->evalLongRange(rewald)/epsilon;
    u += longRange*tau;
  }
  return u;
}

double CoulombAction::getEField(const Paths& paths, int ipart, 
                                int islice) const {
  double u=0,utau=0,ulambda=0;
  Vec fm=0.,fp=0.;
  for (unsigned int i=0; i<pairActionArray.size(); ++i) {
    double ui=0, utaui=0, ulambdai=0; 
    Vec fmi=0.0, fpi=0.0; 
    pairActionArray[i]->
      getBeadAction(paths,ipart,islice,ui,utaui,ulambdai,fmi,fpi);
    u+=ui; utau+=utaui; ulambda+=ulambdai; fm+=fmi; fp+=fpi;
  }
  return fp[0]/tau;
}

double CoulombAction::u(double r, int order) const {
	r = sqrt(r * r + displace2);
	double taueff = 2.0 * mu * q1q2 * q1q2 * tau;
	double reff = 2.0 * mu * q1q2 * r;
	double stau = q1q2 * sqrt(2.0 * mu * tau);
	// First compute the 1D actions.
	Coulomb1DLinkAction coulomb1D(stau);
	double u0 = coulomb1D.calculateValueAtOrigin();
    double u1_0 = coulomb1D.calculateU0(u0, reff, taueff);
	double u1_1 = coulomb1D.calculateU1(stau, reff);
	double u1_2 = coulomb1D.calculateU2(stau, reff);
	double u1_3 = coulomb1D.calculateU3(stau, reff);
	double u1_4 = coulomb1D.calculateU4(stau, reff);

	// Then extract the 3D actions.
    double a = 2. * taueff / (reff * reff);
    double b = exp(-2. / a);
    double c = 1. / (1 + 2 * a * (1 - b) * u1_1);

	double u = 0;
    switch (order) {
    case 0:
        u = u1_0 + log(c);
    	break;
    case 1:
    	u = u1_1 + c * (b * u1_1 - 4 * a * (1 - b) * u1_2);
    	break;
    case 2:
    	u = u1_2 + 0.25 * c / a * (c * (b * u1_1 * (1 + 2 * a * u1_1) + 8 * a * b * u1_2 + 32 * a * a * a * (1 - b) * (1 - b) * u1_2 * u1_2) - 24 * a * a * (1 - b) * u1_3);
    	break;
    case 3:
    	u = u1_3 + c * c * c / (24. * a * a) * (128 * a * a * a * a * a * b * b * b * (4 * u1_2 * u1_2 * u1_2 - 9 * u1_1 * u1_2 * u1_3 + 6 * u1_1 * u1_1 * u1_4) + 64 * a * a * a * (a * u1_2 * (-8 * a * u1_2 * u1_2 + 9 * (1 + 2 * a * u1_1) * u1_3) - 3 * (1 + 2 * a * u1_1) * (1 + 2 * a * u1_1) * u1_4) + 2 * a * b * b * (2 * a * u1_1 * u1_1 * u1_1 + 96 * a * a * u1_2 * (u1_2 - 8 * a * a * u1_2 * u1_2 + 3 * a * u1_3) + 12 * a * u1_1 * (u1_2 - 6 * a * u1_3 + 144 * a * a * a * u1_2 * u1_3 - 32 * a * a * u1_4) + u1_1 * u1_1 * (1 - 1152 * a * a * a * a * u1_4)) + b * (4 * a * a * u1_1 * u1_1 * u1_1 + 12 * a * (u1_2 - 16 * a * a * u1_2 * u1_2 + 128 * a * a * a * a * u1_2 * u1_2 * u1_2 + 6 * a * u1_3 - 96 * a * a * a * u1_2 * u1_3 + 16 * a * a * u1_4) + 4 * u1_1 * u1_1 * (a + 576 * a * a * a * a * a * u1_4) + u1_1 * (1 + 24 * a * a * (u1_2 + 6 * a * u1_3 - 144 * a * a * a * u1_2 * u1_3 + 64 * a * a * u1_4))));
    	break;
    case 4:
    	u = u1_4; //Not exact
    	break;
    }

    if(order == 0 && ewaldSum){
            u -= tau * q1q2 * ewaldSum->evalFR(r);
    }

    return u;
}

double CoulombAction::utau(double r, int order) const {
  double tausave=tau;
  const double eps=1e-7;
  tau = tausave*(1.0+eps);
  double up=u(r,order);
  tau = tausave*(1.0-eps);
  double um=u(r,order);
  tau = tausave;
  return (up-um)/(2.0*eps*tau);
}
