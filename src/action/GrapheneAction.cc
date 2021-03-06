#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "GrapheneAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <blitz/tinyvec-et.h>

GrapheneAction::GrapheneAction(const SimulationInfo &simInfo) 
  : tau(simInfo.getTau()) {
}

double GrapheneAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  double ktstride = 0.5*tau*nStride;
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice);
      cell.pbc(delta);
      deltaAction+=dot(delta,delta)*ktstride;
      // Subtract action for old beads.
      delta=sectionBeads(i,islice);
      cell.pbc(delta);
      deltaAction-=dot(delta,delta)*ktstride;
    }
  }
  return deltaAction;
}

double GrapheneAction::getTotalAction(const Paths& paths, 
    const int level) const {
  return 0;
}

void GrapheneAction::getBeadAction(const Paths& paths, int ipart, int islice,
     double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  Vec delta=paths(ipart,islice);
  utau=0.5*dot(delta,delta);
  u=utau*tau;
  fm=-0.5*tau*delta;
  fp=-0.5*tau*delta;
}
