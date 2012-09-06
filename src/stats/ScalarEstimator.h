#ifndef __ScalarEstimator_h_
#define __ScalarEstimator_h_
#include "Estimator.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
class Paths;
class EstimatorReportBuilder;
template <class T> class ReportWriterInterface;

/// Base class for scalar estimators.
/// @author John Shumway
class ScalarEstimator: public Estimator {
public:
    ScalarEstimator(const std::string& name);
    ScalarEstimator(const std::string &name, const std::string &typeString,
            const std::string &unitName, double shift, double scale);
    virtual ~ScalarEstimator();

    virtual double calcValue()=0;
    double getValue() const;
    void setValue(double value);

    virtual void reset()=0;
    virtual void averageOverClones(const MPIManager* mpi);

    virtual void startReport(EstimatorReportBuilder& builder);

    virtual void reportStep(EstimatorReportBuilder& builder);
protected:
    double value;
    const double scale;
    const double shift;
};
#endif
