#ifndef __ArrayBlockedEstimator_h_
#define __ArrayBlockedEstimator_h_
#include "Estimator.h"
#include "EstimatorReportBuilder.h"
#include "ReportWriters.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
class Paths;
/// Base class for array estimators.
/// @author John Shumway
class ArrayEstimator : public Estimator {
public:
  ArrayEstimator(const std::string &name, 
                        const std::string &typeString, bool hasError);
  virtual ~ArrayEstimator();

  virtual void reset()=0;

  virtual void startReport(ReportWriters *writers) {
    writers->startArrayReport(this, 0);}

  virtual void reportStep(ReportWriters *writers) {
    writers->reportArrayStep(this, 0);}

  virtual int getNDim() const=0;

  virtual int getExtent(const int idim) const=0;

  virtual const void* getData() const=0;
  virtual const void* getError() const=0;
  bool hasError() const {return hasErrorFlag;}

  virtual void normalize() const=0;
  virtual void unnormalize() const=0;
  virtual bool hasScale() const=0;
  virtual bool hasOrigin() const=0;
  virtual const void* getScale() const=0;
  virtual const void* getOrigin() const=0;
  virtual bool hasMin() const=0;
  virtual bool hasMax() const=0;
  virtual const void* getMin() const=0;
  virtual const void* getMax() const=0;
  protected:
  bool hasErrorFlag;
};
#endif
