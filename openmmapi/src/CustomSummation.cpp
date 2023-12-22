/* -------------------------------------------------------------------------- *
 *                             OpenMM Laboratory                              *
 *                             =================                              *
 *                                                                            *
 * A plugin for testing low-level code implementation for OpenMM.             *
 *                                                                            *
 * Copyright (c) 2023 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-lab                                     *
 * -------------------------------------------------------------------------- */

#include "CustomSummation.h"
#include "openmm/Context.h"
#include "openmm/OpenMMException.h"
#include "openmm/Platform.h"
#include "openmm/State.h"
#include "openmm/System.h"
#include "openmm/Vec3.h"
#include "openmm/VerletIntegrator.h"
#include "openmm/internal/AssertionUtilities.h"
#include "openmm/internal/ContextImpl.h"

#include <map>
#include <regex>
#include <string>

#include <iostream>  // temporary

#define ASSERT_INDEX(index, num) { \
    if (index < 0 || index >= num) \
        throwException(__FILE__, __LINE__, "Index out of range"); \
};

using namespace OpenMM;
using namespace OpenMMLab;
using namespace std;

CustomSummation::Evaluator::Evaluator(
    int numArgs,
    CustomCompoundBondForce &force,
    Platform &platform,
    const map<string, string> &properties
) : numArgs(numArgs) {
    int numParticles = force.getNumParticlesPerBond();
    positions.resize(numParticles, Vec3(0, 0, 0));
    latestArguments.resize(numArgs);
    derivatives.resize(numArgs);

    System *system = new System();
    for (int i = 0; i < numParticles; i++)
        system->addParticle(1.0);
    system->addForce(static_cast<Force *>(&force));

    VerletIntegrator *integrator = new VerletIntegrator(0.01);
    context = new Context(*system, *integrator, platform, properties);
    valueIsDirty = derivativesAreDirty = contextIsUnchanged = true;
};

CustomSummation::Evaluator::~Evaluator() {
    delete context; // will delete the system, integrator, and force
}

void CustomSummation::Evaluator::setPositions(const vector<double> &arguments) {
    if (equal(arguments.begin(), arguments.end(), latestArguments.begin()) && contextIsUnchanged)
        return;
    for (int i = 0; i < numArgs; i++)
        positions[i / 3][i % 3] = arguments[i];
    context->setPositions(positions);
    latestArguments = arguments;
    valueIsDirty = derivativesAreDirty = contextIsUnchanged = true;
}

double CustomSummation::Evaluator::evaluate(const vector<double> &arguments) {
    setPositions(arguments);
    if (valueIsDirty) {
        value = context->getState(State::Energy).getPotentialEnergy();
        valueIsDirty = false;
    }
    return value;
}

vector<double> CustomSummation::Evaluator::evaluateDerivatives(const vector<double> &arguments) {
    setPositions(arguments);
    if (derivativesAreDirty) {
        vector<Vec3> forces = context->getState(State::Forces).getForces();
        for (int i = 0; i < numArgs; i++)
            derivatives[i] = -forces[i / 3][i % 3];
        derivativesAreDirty = false;
    }
    return derivatives;
}

void CustomSummation::Evaluator::update(CustomCompoundBondForce &force) {
    force.updateParametersInContext(*context);
    contextIsUnchanged = false;
}

void CustomSummation::Evaluator::reset() {
    context->reinitialize();
    contextIsUnchanged = false;
}

double CustomSummation::Evaluator::getParameter(const string &name) const {
    return context->getParameter(name);
}

void CustomSummation::Evaluator::setParameter(const string &name, double value) {
    context->setParameter(name, value);
    contextIsUnchanged = false;
}

const map<string, string> &CustomSummation::Evaluator::getPlatformProperties() const {
    Platform &platform = context->getPlatform();
    vector<string> names = platform.getPropertyNames();
    map<string, string> &properties = *new map<string, string>;
    for (const auto& name : names)
        properties[name] = platform.getPropertyValue(*context, name);
    return properties;
}

CustomSummation::CustomSummation(
    int numArgs,
    const string &expression,
    const map<string, double> &overallParameters,
    const vector<string> &perTermParameterNames,
    Platform &platform,
    const map<string, string> &properties
) : numArgs(numArgs), platform(&platform) {
    int numParticles = (numArgs  + 2)/ 3;
    for (int i = 0; i < numParticles; i++)
        particles.push_back(i);
    force = new CustomCompoundBondForce(numParticles, expression);
    force->setUsesPeriodicBoundaryConditions(false);
    for (const auto& pair : overallParameters)
        force->addGlobalParameter(pair.first, pair.second);
    for (const auto& name : perTermParameterNames)
        force->addPerBondParameter(name);
    evaluator = new Evaluator(numArgs, *force, platform, properties);
}

CustomSummation::~CustomSummation() {
    delete evaluator;  // will delete the force
}

double CustomSummation::evaluate(const double* arguments) const {
    return evaluator->evaluate(vector<double>(arguments, arguments + numArgs));
}

double CustomSummation::evaluate(const vector<double> &arguments) const {
    return evaluator->evaluate(arguments);
}

double CustomSummation::evaluateDerivative(const double* arguments, const int* derivOrder) const {
    vector<int> order(derivOrder, derivOrder + numArgs);
    int which;
    int acc = 0;
    for (int i = 0; i < numArgs; i++) {
        int item = order[i];
        if (item < 0 || (acc += item) > 1)
            throwException(__FILE__, __LINE__, "Invalid derivative order specification");
        if (item == 1)
            which = i;
    }
    vector<double> args(arguments, arguments + numArgs);
    return evaluator->evaluateDerivatives(args)[which];
}

double CustomSummation::evaluateDerivative(const vector<double> &arguments, int which) const {
    return evaluator->evaluateDerivatives(arguments)[which];
}

CustomSummation* CustomSummation::clone() const {
    map<string, double> overallParameters;
    for (int i = 0; i < getNumOverallParameters(); i++) {
        string name = getOverallParameterName(i);
        overallParameters[name] = getOverallParameterDefaultValue(i);
    }
    vector<string> perTermParameterNames;
    for (int i = 0; i < getNumPerTermParameters(); i++)
        perTermParameterNames.push_back(getPerTermParameterName(i));
    CustomSummation *copy = new CustomSummation(
        numArgs,
        getExpression(),
        overallParameters,
        perTermParameterNames,
        getPlatform(),
        getPlatformProperties()
    );
    for (int i = 0; i < getNumTerms(); i++)
        copy->addTerm(getTerm(i));
    for (int i = 0; i < getNumOverallParameters(); i++) {
        string name = getOverallParameterName(i);
        copy->setParameter(name, getParameter(name));
    }
    return copy;
}

int CustomSummation::getNumOverallParameters() const {
    return force->getNumGlobalParameters();
}

const string& CustomSummation::getOverallParameterName(int index) const {
    ASSERT_INDEX(index, force->getNumGlobalParameters());
    return force->getGlobalParameterName(index);
}

double CustomSummation::getOverallParameterDefaultValue(int index) const {
    ASSERT_INDEX(index, force->getNumGlobalParameters());
    return evaluator->getParameter(force->getGlobalParameterName(index));
}

int CustomSummation::getNumPerTermParameters() const {
    return force->getNumPerBondParameters();
}

const string& CustomSummation::getPerTermParameterName(int index) const {
    ASSERT_INDEX(index, force->getNumPerBondParameters());
    return force->getPerBondParameterName(index);
}

const map<string, string> &CustomSummation::getPlatformProperties() const {
    return evaluator->getPlatformProperties();
}

int CustomSummation::addTerm(const vector<double> &parameters) {
    force->addBond(particles, parameters);
    evaluator->reset();
    return force->getNumBonds() - 1;
}

const vector<double> &CustomSummation::getTerm(int index) const {
    ASSERT_INDEX(index, force->getNumBonds());
    vector<int> _;
    vector<double> *parameters = new vector<double>(getNumPerTermParameters());
    force->getBondParameters(index, _, *parameters);
    return *parameters;
}

void CustomSummation::setTerm(int index, const vector<double> &parameters) {
    ASSERT_INDEX(index, force->getNumBonds());
    force->setBondParameters(index, particles, parameters);
    evaluator->update(*force);
}

double CustomSummation::getParameter(const string &name) const {
    return evaluator->getParameter(name);
}

void CustomSummation::setParameter(const string &name, double value) {
    evaluator->setParameter(name, value);
}
