#ifndef OPENMMLAB_CUSTOMSUMMATION_H_
#define OPENMMLAB_CUSTOMSUMMATION_H_

/* -------------------------------------------------------------------------- *
 *                             OpenMM Laboratory                              *
 *                             =================                              *
 *                                                                            *
 * A plugin for testing low-level code implementation for OpenMM.             *
 *                                                                            *
 * Copyright (c) 2023 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-lab                                     *
 * -------------------------------------------------------------------------- */

#include "internal/windowsExportOpenMMLab.h"
#include "openmm/Context.h"
#include "openmm/CustomCompoundBondForce.h"
#include "openmm/Platform.h"
#include "lepton/CustomFunction.h"
#include "openmm/internal/ContextImpl.h"
#include <map>
#include <string>
#include <vector>

using namespace OpenMM;
using namespace Lepton;
using namespace std;

namespace OpenMMLab {

/*
* A CustomSummation...
*/

class OPENMM_EXPORT_OPENMM_LAB CustomSummation : public Lepton::CustomFunction {
public:
    /**
     * Construct a new CustomSummation object.
     *
     * @param numArgs                the number of arguments
     * @param expression             the expression for each term in the summation
     * @param overallParameters      the names and default values of the parameters that
     *                               are shared by all terms of the summation. Not to be
     *                               confused with global context parameters
     * @param perTermParameterNames  the names of the parameters that are unique to each
     *                               term of the summation
     * @param platform               the platform that will be used to evaluate the
     *                               summation
     * @param properties             a set of values for platform-specific properties
     */
    CustomSummation(
        int numArgs,
        const std::string &expression,
        const map<string, double> &overallParameters,
        const vector<string> &perTermParameterNames,
        Platform &platform,
        const map<string, string> &properties = map<string, string>()
    );
    ~CustomSummation();
    /**
     * Get the number of arguments this function expects.
     */
    int getNumArguments() const { return numArgs; }
    /**
     * Evaluate the function.
     *
     * @param arguments    an array of argument values
     */
    double evaluate(const double *arguments) const;
    /**
     * Evaluate the function.
     *
     * @param arguments    a vector of argument values
     */
    double evaluate(const vector<double> &arguments) const;
    /**
     * Evaluate a derivative of the function.
     *
     * @param arguments    the array of argument values
     * @param derivOrder   an array specifying the number of times the function has been differentiated
     *                     with respect to each of its arguments.  For example, the array {0, 2} indicates
     *                     a second derivative with respect to the second argument.
     */
    double evaluateDerivative(const double *arguments, const int *derivOrder) const;
    /**
     * Evaluate a derivative of the function.
     *
     * @param arguments    a vector of argument values
     * @param which        the index of the argument for which to evaluate the derivative
     */
    double evaluateDerivative(const vector<double> &arguments, int which) const;
    /**
     * Create a new duplicate of this object on the heap using the "new" operator.
     */
    CustomSummation *clone() const;
    /**
     * Get the expression for each term of the summation.
     *
     * @return         the expression
     */
    const string &getExpression() const { return force->getEnergyFunction(); }
    /**
     * Get the number of overall parameters.
     *
     * @return         the number of overall parameters
     */
    int getNumOverallParameters() const;
    /**
     * Get the name of a overall parameter.
     *
     * @param index    the index of the overall parameter for which to get the name
     * @return         the overall parameter name
     */
    const std::string &getOverallParameterName(int index) const;
    /**
     * Get the value of a overall parameter.
     *
     * @param index    the index of the overall parameter for which to get the value
     * @return         the overall parameter value
     */
    double getOverallParameterDefaultValue(int index) const;
    /**
     * Get the number of per-term parameters.
     *
     * @return         the number of per-term parameters
     */
    int getNumPerTermParameters() const;
    /**
     * Get the name of a per-term parameter.
     *
     * @param index    the index of the per-term parameter for which to get the name
     * @return         the per-term parameter name
     */
    const std::string &getPerTermParameterName(int index) const;
    /**
     * Get the platform that will be used to evaluate the summation.
     *
     * @return         the platform
     */
    Platform &getPlatform() const { return *platform; }
    /**
     * Get the platform properties.
    */
    const map<string, string> &getPlatformProperties() const;
    /**
     * Add a new term to the summation.
     *
     * @param parameters    the parameters of the term
     * @return              the index of the new term
     */
    int addTerm(vector<double> parameters);
    /**
     * Get the number of terms in the summation.
     *
     * @return         the number of terms
     */
    int getNumTerms() const { return force->getNumBonds(); }
    /**
     * Get the parameters of a term.
     *
     * @param index    the index of the term
     * @return         the parameters of the term
    */
    vector<double> getTerm(int index) const;
    /**
     * Set the parameters of a term.
     *
     * @param index    the index of the term
     * @param parameters    the parameters of the term
     */
    void setTerm(int index, vector<double> parameters);
    /**
     * Get the value of an overall parameter.
     *
     * @param name    the name of the parameter
     * @return        the value of the parameter
     */
    double getParameter(const string &name) const;
    /**
     * Set the value of an overall parameter.
     *
     * @param name    the name of the parameter
     * @param value   the value of the parameter
     */
    void setParameter(const string &name, double value);
private:
    class Evaluator;
    int numArgs;
    vector<int> particles;
    CustomCompoundBondForce *force;
    Evaluator *evaluator;
    Platform *platform;
};

class CustomSummation::Evaluator {
public:
    Evaluator(
        int numArgs,
        CustomCompoundBondForce &force,
        Platform &platform,
        const map<string, string> &properties
    );
    ~Evaluator();
    double evaluate(vector<double> arguments);
    vector<double> evaluateDerivatives(vector<double> arguments);
    void update(CustomCompoundBondForce &force);
    void reset();
    double getParameter(const string &name) const;
    void setParameter(const string &name, double value);
    const map<string, string> &getPlatformProperties() const;
private:
    void setPositions(vector<double> arguments);
    int numArgs;
    Context *context;
    bool contextIsUnchanged;
    vector<Vec3> positions;
    vector<double> latestArguments;
    double value;
    bool valueIsDirty;
    vector<double> derivatives;
    bool derivativesAreDirty;
};

} // namespace OpenMMLab

#endif /*OPENMMLAB_CUSTOMSUMMATION_H_*/
