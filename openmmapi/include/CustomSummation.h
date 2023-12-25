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

#include "internal/CustomSummationImpl.h"
#include "internal/windowsExportOpenMMLab.h"
#include "openmm/Platform.h"
#include "lepton/CustomFunction.h"

#include <map>
#include <string>
#include <vector>

using namespace OpenMM;
using namespace Lepton;
using namespace std;

namespace OpenMMLab {

/**
 * This class allows users to define a custom function that can be evaluated in an
 * OpenMM::Platform. It defines a sum that depends of a fixed number of arguments,
 * a set of per-term parameters, and a set of overall parameters.
 *
 * We refer to the arguments as x1, y1, z1, x2, y2, z2, x3, y3, etc. CustomSummation
 * evaluates a user supplied algebraic expression to determine the value of each term.
 * The expression may depend on the following variables and functions:
 *
 * <ul>
 * <li>x1, y1, z1, x2, y2, z2, x3, etc.: the argument passed to the function.</li>
 * <li>p1, p2, p3, etc.: three-dimensional points defined as (x1, y1, z1), (x2, y2, z2),
 * etc. If the number of arguments is not a multiple of 3, the last point is completed
 * with zeros.</li>
 * <li>distance(p1, p2): the distance between points p1 and p2 (where "p1" and "p2"
 * may be replaced by any valid point names.</li>
 * <li>angle(p1, p2, p3): the angle formed by the three specified points.</li>
 * <li>dihedral(p1, p2, p3, p4): the dihedral angle formed by the four specified
 * points, guaranteed to be in the range [-pi,+pi].</li>
 * </ul>
 *
 * To use this class, create a CustomSummation object, passing the following data to
 * the constructor:
 *
 * <ul>
 * <li>the number of arguments</li>
 * <li>an algebraic expression that defines each term of the sum</li>
 * <li>a map of overall parameter names to default values</li>
 * <li>a list of per-term parameter names</li>
 * <li>the OpenMM::Platform to use for calculations</li>
 * <li>a map of platform-specific property names to values</li>
 * </ul>
 *
 * Then, call addTerm() to define terms of the sum and specify their parameter values.
 * After a term has been added, you can modify its parameters by calling setTerm().
 *
 * As an example, the following code creates a CustomSummation that evaluates a
 * Gaussian mixture in a three-dimensional space. All kernels have the same standard
 * deviation, but different means. A kernel is added for each vertex of a unit cube.
 * Then, the sum is evaluated for a point in the middle of the cube.
 *
 * \verbatim embed:rst:leading-asterisk
 * .. code-block:: cpp
 *
 *     CustomSummation* function = new CustomSummation(
 *         3,
 *         "exp(-((x1-mux)^2+(y1-muy)^2+(z1-muz)^2)/(2*sigma^2))/sqrt(6.2832*sigma^2)",
 *         map<string, double>{{"sigma", 1.0}},
 *         vector<string>{"mux", "muy", "muz"},
 *         Platform::getPlatformByName("CUDA")
 *     );
 *
 *     function->addTerm(vector<double>{0.0, 0.0, 0.0});
 *     function->addTerm(vector<double>{0.0, 0.0, 1.0});
 *     function->addTerm(vector<double>{0.0, 1.0, 0.0});
 *     function->addTerm(vector<double>{0.0, 1.0, 1.0});
 *     function->addTerm(vector<double>{1.0, 0.0, 0.0});
 *     function->addTerm(vector<double>{1.0, 0.0, 1.0});
 *     function->addTerm(vector<double>{1.0, 1.0, 0.0});
 *     function->addTerm(vector<double>{1.0, 1.0, 1.0});
 *
 *     double value = function->evaluate(vector<double>{0.5, 0.5, 0.5});
 *
 * \endverbatim
 *
 * This class also has the ability to compute derivatives of the sum with respect to
 * the arguments.
 *
 * Expressions may involve the operators + (add), - (subtract), * (multiply),
 * / (divide), and ^ (power), and the following functions: sqrt, exp, log, sin, cos,
 * sec, csc, tan, cot, asin, acos, atan, atan2, sinh, cosh, tanh, erf, erfc, min, max,
 * abs, floor, ceil, step, delta, select.  All trigonometric functions are defined in
 * radians, and log is the natural logarithm.  step(x) = 0 if x is less than 0,
 * 1 otherwise.  delta(x) = 1 if x is 0, 0 otherwise. select(x,y,z) = z if x = 0,
 * y otherwise.
 *
 * This class also supports the functions pointdistance(), pointangle(), and
 * pointdihedral(), which accept 6, 9, and 12 arguments, respectively.  These functions
 * are similar to distance(), angle(), and dihedral(), but their arguments can be any
 * evaluatable expressions rather than the names of predefined points like p1, p2, p3,
 * etc. For example, the following computes the distance from point p1 to the midpoint
 * between p2 and p3.
 *
 * \verbatim embed:rst:leading-asterisk
 * .. code-block:: cpp
 *
 *    string expression = "pointdistance(x1, y1, z1, (x2+x3)/2, (y2+y3)/2, (z2+z3)/2)";
 *
 * \endverbatim
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
     * @param perTermParameters      the names of the parameters that are unique to each
     *                               term of the summation
     * @param platform               the platform that will be used to evaluate the
     *                               summation
     * @param platformProperties     a set of values for platform-specific properties
     */
    CustomSummation(
        int numArgs,
        string expression,
        map<string, double> overallParameters,
        vector<string> perTermParameters,
        Platform &platform,
        map<string, string> platformProperties = map<string, string>()
    );
    ~CustomSummation();
    /**
     * Get the number of arguments this function expects.
     */
    int getNumArguments() const { return numArgs; }
    /**
     * Evaluate the function.
     *
     * @param arguments   an array of argument values
     * @returns           the value of the function
     */
    double evaluate(const double *arguments) const;
    /**
     * Evaluate the function.
     *
     * @param arguments    a vector of argument values
     * @returns            the value of the function
     */
    double evaluate(const vector<double> &arguments) const;
    /**
     * Evaluate a derivative of the function.
     *
     * @param arguments    the array of argument values
     * @param derivOrder   an array specifying the number of times the function has been differentiated
     *                     with respect to each of its arguments.  For example, the array {0, 2} indicates
     *                     a second derivative with respect to the second argument.
     * @returns            the value of the derivative
     */
    double evaluateDerivative(const double *arguments, const int *derivOrder) const;
    /**
     * Evaluate a derivative of the function.
     *
     * @param arguments    a vector of argument values
     * @param which        the index of the argument for which to evaluate the derivative
     * @returns            the value of the derivative
     */
    double evaluateDerivative(const vector<double> &arguments, int which) const;
    /**
     * Create a new duplicate of this object on the heap using the "new" operator.
     */
    CustomSummation *clone() const;
    /**
     * Get the expression for each term of the summation.
     */
    const string &getExpression() const { return expression; }
    /**
     * Get a map of the names to default values of the overall parameters.
     */
    const map<string, double> &getOverallParameters() const { return overallParameters; }
    /**
     * Get a vector of the names of the per-term parameters.
     */
    const vector<string> &getPerTermParameters() const { return perTermParameters; }
    /**
     * Get the platform used to evaluate the summation.
    */
    Platform &getPlatform() { return *platform; }
    /**
     * Get the properties of the platform used to evaluate the summation.
     */
    const map<string, string> &getPlatformProperties() const { return platformProperties; }
    /**
     * Add a new term to the summation.
     *
     * @param parameters    the parameters of the term
     * @return              the index of the new term
     */
    int addTerm(const vector<double> &parameters);
    /**
     * Get the number of terms in the summation.
     */
    int getNumTerms() const { return termParameters.size(); }
    /**
     * Get the parameters of a term.
     *
     * @param index    the index of the term
    */
    const vector<double> &getTerm(int index) const;
    /**
     * Set the parameters of a term.
     *
     * @param index         the index of the term
     * @param parameters    the new parameters for the term
     */
    void setTerm(int index, const vector<double> &parameters);
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
    /**
     * Update the custom summation after new terms have been added or
     * parameters of existing terms have been modified.
     */
    void update();
private:
    int numArgs;
    string expression;
    map<string, double> overallParameters;
    vector<string> perTermParameters;
    vector<vector<double>> termParameters;
    Platform *platform;
    map<string, string> platformProperties;
    CustomSummationImpl *impl;
};

} // namespace OpenMMLab

#endif /*OPENMMLAB_CUSTOMSUMMATION_H_*/
