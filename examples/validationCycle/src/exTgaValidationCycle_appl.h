/*--------------------------------------------------------------------------
 *--------------------------------------------------------------------------
 *
 * Copyright (C) 2008 The PECOS Development Team
 *
 * Please see http://pecos.ices.utexas.edu for more information.
 *
 * This file is part of the QUESO Library (Quantification of Uncertainty
 * for Estimation, Simulation and Optimization).
 *
 * QUESO is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QUESO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QUESO. If not, see <http://www.gnu.org/licenses/>.
 *
 *--------------------------------------------------------------------------
 *
 * $Id$
 *
 * Brief description of this file: 
 * 
 *--------------------------------------------------------------------------
 *-------------------------------------------------------------------------- */

#ifndef __EX_TGA_VALIDATION_CYCLE_APPL_H__
#define __EX_TGA_VALIDATION_CYCLE_APPL_H__

#define UQ_EXAMPLES_USES_QUESO_INPUT_FILE

#include <exTgaValidationCycle_likelihood.h>
#include <exTgaValidationCycle_qoi.h>
#include <uqValidationCycle.h>
#include <uqVectorSubset.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_odeiv.h>

//Just declaration: actual code is below
template<class P_V,class P_M,class Q_V,class Q_M>
void 
uqAppl_LocalComparisonStage(uqValidationCycleClass<P_V,P_M,Q_V,Q_M>& cycle);

template<class P_V,class P_M,class Q_V,class Q_M>
void 
uqAppl_UnifiedComparisonStage(uqValidationCycleClass<P_V,P_M,Q_V,Q_M>& cycle);

//********************************************************
// The driving routine "uqAppl()": called by main()
// There are 5 main tasks:
// 1) initilization
// 2) the 'calibration stage'
// 3) the 'validation stage'
// 4) the 'comparison stage'
// 5) memory release
// Tasks 2, 3 and 4 constitute the actual validation cycle.
//********************************************************
template<class P_V,class P_M,class Q_V,class Q_M>
void 
uqAppl(const uqBaseEnvironmentClass& env)
{
  if (env.fullRank() == 0) {
    std::cout << "Beginning run of 'uqTgaExample' example\n"
              << std::endl;
  }

  int iRC;
  struct timeval timevalRef;
  struct timeval timevalNow;

  //******************************************************
  // Task 1 of 5: instantiation of basic classes
  //******************************************************

  // Instantiate the parameter space
  std::vector<std::string> paramNames(2,"");
  paramNames[0] = "A_param";
  paramNames[1] = "E_param";
  uqVectorSpaceClass<P_V,P_M> paramSpace(env,"param_",paramNames.size(),&paramNames);

  // Instantiate the parameter domain
  P_V paramMinValues(paramSpace.zeroVector());
  paramMinValues[0] = 2.40e+11;
  paramMinValues[1] = 1.80e+05;
  P_V paramMaxValues(paramSpace.zeroVector());
  paramMaxValues[0] = 2.80e+11;
  paramMaxValues[1] = 2.20e+05;
  uqBoxSubsetClass<P_V,P_M> paramDomain("param_",
                                        paramSpace,
                                        paramMinValues,
                                        paramMaxValues);

  // Instantiate the qoi space
  std::vector<std::string> qoiNames(1,"");
  qoiNames[0] = "TimeFor25PercentOfMass";
  uqVectorSpaceClass<Q_V,Q_M> qoiSpace(env,"qoi_",qoiNames.size(),&qoiNames);

  // Instantiate the validation cycle
  uqValidationCycleClass<P_V,P_M,Q_V,Q_M> cycle(env,
                                                "", // No extra prefix
                                                paramSpace,
                                                qoiSpace);

  //********************************************************
  // Task 2 of 5: calibration stage
  //********************************************************

  iRC = gettimeofday(&timevalRef, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Beginning 'calibration stage' at " << ctime(&timevalRef.tv_sec)
              << std::endl;
  }

  // Inverse problem: instantiate the prior rv
  uqUniformVectorRVClass<P_V,P_M> calPriorRv("cal_prior_", // Extra prefix before the default "rv_" prefix
                                             paramDomain);

  // Inverse problem: instantiate the likelihood function object (data + routine)
  likelihoodRoutine_DataClass<P_V,P_M> calLikelihoodRoutine_Data(env,
                                                                 "inputData/scenario_5_K_min.dat",
                                                                 "inputData/scenario_25_K_min.dat",
                                                                 "inputData/scenario_50_K_min.dat");

  uqGenericScalarFunctionClass<P_V,P_M> calLikelihoodFunctionObj("cal_like_",
                                                                 paramDomain,
                                                                 likelihoodRoutine<P_V,P_M>,
                                                                 (void *) &calLikelihoodRoutine_Data,
                                                                 true); // the routine computes [ln(function)]

  // Inverse problem: instantiate it (posterior rv is instantiated internally)
  uqSipOptionsValuesClass* calIpOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  calIpOptionsValues = new uqSipOptionsValuesClass();
  calIpOptionsValues->m_computeSolution      = true;
  calIpOptionsValues->m_dataOutputFileName   = "outputData/tgaCalOutput";
  calIpOptionsValues->m_dataOutputAllowedSet.insert(0);
  calIpOptionsValues->m_dataOutputAllowedSet.insert(1);
#endif
  cycle.instantiateCalIP(calIpOptionsValues,
                         calPriorRv,
                         calLikelihoodFunctionObj);

  // Inverse problem: solve it, that is, set 'pdf' and 'realizer' of the posterior rv
  P_V paramInitialValues(paramSpace.zeroVector());
  if (env.numSubEnvironments() == 1) {
    // For regression test purposes
    paramInitialValues[0] = 2.41e+11;
    paramInitialValues[1] = 2.19e+05;
  }
  else {
    calPriorRv.realizer().realization(paramInitialValues);
  }

  uqMhOptionsValuesClass* calIpMhOptionsValues = NULL;
  P_M* calProposalCovMatrix = cycle.calIP().postRv().imageSet().vectorSpace().newProposalMatrix(NULL,&paramInitialValues);
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  uqSsOptionsValuesClass ssOptionsValues1;
  uqSsOptionsValuesClass ssOptionsValues2;

  ssOptionsValues1.m_initialDiscardedPortions.resize(9);
  ssOptionsValues1.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues1.m_initialDiscardedPortions[1] = 0.05;
  ssOptionsValues1.m_initialDiscardedPortions[2] = 0.10;
  ssOptionsValues1.m_initialDiscardedPortions[3] = 0.15;
  ssOptionsValues1.m_initialDiscardedPortions[4] = 0.20;
  ssOptionsValues1.m_initialDiscardedPortions[5] = 0.25;
  ssOptionsValues1.m_initialDiscardedPortions[6] = 0.30;
  ssOptionsValues1.m_initialDiscardedPortions[7] = 0.35;
  ssOptionsValues1.m_initialDiscardedPortions[8] = 0.40;
  ssOptionsValues1.m_bmmRun                      = false;
  ssOptionsValues1.m_fftCompute                  = false;
  ssOptionsValues1.m_psdCompute                  = false;
  ssOptionsValues1.m_psdAtZeroCompute            = false;
  ssOptionsValues1.m_gewekeCompute               = true;
  ssOptionsValues1.m_gewekeNaRatio               = .1;
  ssOptionsValues1.m_gewekeNbRatio               = .5;
  ssOptionsValues1.m_gewekeDisplay               = true;
  ssOptionsValues1.m_gewekeWrite                 = true;
  ssOptionsValues1.m_autoCorrComputeViaDef       = false;
  ssOptionsValues1.m_autoCorrComputeViaFft       = true;
  ssOptionsValues1.m_autoCorrSecondLag           = 2;
  ssOptionsValues1.m_autoCorrLagSpacing          = 2;
  ssOptionsValues1.m_autoCorrNumLags             = 15;
  ssOptionsValues1.m_autoCorrDisplay             = true;
  ssOptionsValues1.m_autoCorrWrite               = true;
  ssOptionsValues1.m_meanStaccCompute            = false;
  ssOptionsValues1.m_histCompute                 = false;
  ssOptionsValues1.m_cdfStaccCompute             = false;
  ssOptionsValues1.m_kdeCompute                  = false;
  ssOptionsValues1.m_covMatrixCompute            = true;
  ssOptionsValues1.m_corrMatrixCompute           = true;

  ssOptionsValues2.m_initialDiscardedPortions.resize(1);
  ssOptionsValues2.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues2.m_bmmRun                      = false;
  ssOptionsValues2.m_fftCompute                  = false;
  ssOptionsValues2.m_psdCompute                  = false;
  ssOptionsValues2.m_psdAtZeroCompute            = false;
  ssOptionsValues2.m_gewekeCompute               = false;
  ssOptionsValues2.m_autoCorrComputeViaDef       = false;
  ssOptionsValues2.m_autoCorrComputeViaFft       = true;
  ssOptionsValues2.m_autoCorrSecondLag           = 2;
  ssOptionsValues2.m_autoCorrLagSpacing          = 2;
  ssOptionsValues2.m_autoCorrNumLags             = 15;
  ssOptionsValues2.m_autoCorrDisplay             = true;
  ssOptionsValues2.m_autoCorrWrite               = true;
  ssOptionsValues2.m_meanStaccCompute            = false;
  ssOptionsValues2.m_histCompute                 = true;
  ssOptionsValues2.m_histNumInternalBins         = 250;
  ssOptionsValues2.m_cdfStaccCompute             = false;
  ssOptionsValues2.m_kdeCompute                  = true;
  ssOptionsValues2.m_kdeNumEvalPositions         = 250;
  ssOptionsValues2.m_covMatrixCompute            = true;
  ssOptionsValues2.m_corrMatrixCompute           = true;

  calIpMhOptionsValues = new uqMhOptionsValuesClass(&ssOptionsValues1,&ssOptionsValues2);
  calIpMhOptionsValues->m_dataOutputFileName   = "outputData/tgaCalOutput";
  calIpMhOptionsValues->m_dataOutputAllowedSet.insert(0);
  calIpMhOptionsValues->m_dataOutputAllowedSet.insert(1);

  calIpMhOptionsValues->m_rawChainDataInputFileName     = ".";
  calIpMhOptionsValues->m_rawChainSize                  = 1048576;
  calIpMhOptionsValues->m_rawChainGenerateExtra         = false;
  calIpMhOptionsValues->m_rawChainDisplayPeriod         = 20000;
  calIpMhOptionsValues->m_rawChainMeasureRunTimes       = true;
  calIpMhOptionsValues->m_rawChainDataOutputFileName    = "outputData/file_cal_ip_raw";
  calIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(0);
  calIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(1);
  calIpMhOptionsValues->m_rawChainComputeStats          = true;

  calIpMhOptionsValues->m_displayCandidates         = false;
  calIpMhOptionsValues->m_putOutOfBoundsInChain     = true;
  calIpMhOptionsValues->m_tkUseLocalHessian         = false;
  calIpMhOptionsValues->m_tkUseNewtonComponent      = true;
  calIpMhOptionsValues->m_drMaxNumExtraStages       = 1;
  calIpMhOptionsValues->m_drScalesForExtraStages.resize(1);
  calIpMhOptionsValues->m_drScalesForExtraStages[0] = 5.;
  calIpMhOptionsValues->m_amInitialNonAdaptInterval = 0;
  calIpMhOptionsValues->m_amAdaptInterval           = 100;
  calIpMhOptionsValues->m_amEta                     = 1.92;
  calIpMhOptionsValues->m_amEpsilon                 = 1.e-5;

  calIpMhOptionsValues->m_filteredChainGenerate              = true;
  calIpMhOptionsValues->m_filteredChainDiscardedPortion      = 0.;
  calIpMhOptionsValues->m_filteredChainLag                   = 20;
  calIpMhOptionsValues->m_filteredChainDataOutputFileName    = ".";
  calIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(0);
  calIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(1);
  calIpMhOptionsValues->m_filteredChainComputeStats          = true;
#endif
  cycle.calIP().solveWithBayesMetropolisHastings(calIpMhOptionsValues,
                                                 paramInitialValues,
                                                 calProposalCovMatrix);
  delete calProposalCovMatrix;
  delete calIpMhOptionsValues;

  // Forward problem: instantiate it (parameter rv = posterior rv of inverse problem; qoi rv is instantiated internally)
  double beta_prediction         = 250.;
  double criticalMass_prediction = 0.;
  double criticalTime_prediction = 3.9;

  qoiRoutine_DataClass<P_V,P_M,Q_V,Q_M> calQoiRoutine_Data;
  calQoiRoutine_Data.m_beta         = beta_prediction;
  calQoiRoutine_Data.m_criticalMass = criticalMass_prediction;
  calQoiRoutine_Data.m_criticalTime = criticalTime_prediction;

  uqSfpOptionsValuesClass* calFpOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  calFpOptionsValues = new uqSfpOptionsValuesClass();
  calFpOptionsValues->m_computeSolution      = true;
  calFpOptionsValues->m_computeCovariances   = true;
  calFpOptionsValues->m_computeCorrelations  = true;
  calFpOptionsValues->m_dataOutputFileName   = "outputData/tgaCalOutput";
  calFpOptionsValues->m_dataOutputAllowedSet.insert(0);
  calFpOptionsValues->m_dataOutputAllowedSet.insert(1);
#endif
  cycle.instantiateCalFP(calFpOptionsValues,
                         qoiRoutine<P_V,P_M,Q_V,Q_M>,
                         (void *) &calQoiRoutine_Data);

  // Forward problem: solve it, that is, set 'realizer' and 'cdf' of the qoi rv
  uqMcOptionsValuesClass* calFpMcOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  uqSsOptionsValuesClass ssOptionsValues3;
  uqSsOptionsValuesClass ssOptionsValues4;

  ssOptionsValues3.m_initialDiscardedPortions.resize(1);
  ssOptionsValues3.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues3.m_meanStaccCompute            = false;
  ssOptionsValues3.m_histCompute                 = true;
  ssOptionsValues3.m_histNumInternalBins         = 250;
  ssOptionsValues3.m_cdfStaccCompute             = false;
  ssOptionsValues3.m_kdeCompute                  = true;
  ssOptionsValues3.m_kdeNumEvalPositions         = 250;
  ssOptionsValues3.m_covMatrixCompute            = true;
  ssOptionsValues3.m_corrMatrixCompute           = true;

  ssOptionsValues4.m_initialDiscardedPortions.resize(1);
  ssOptionsValues4.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues4.m_bmmRun                      = false;
  ssOptionsValues4.m_fftCompute                  = false;
  ssOptionsValues4.m_psdCompute                  = false;
  ssOptionsValues4.m_psdAtZeroCompute            = false;
  ssOptionsValues4.m_gewekeCompute               = false;
  ssOptionsValues4.m_autoCorrComputeViaDef       = false;
  ssOptionsValues4.m_autoCorrComputeViaFft       = true;
  ssOptionsValues4.m_autoCorrSecondLag           = 2;
  ssOptionsValues4.m_autoCorrLagSpacing          = 1;
  ssOptionsValues4.m_autoCorrNumLags             = 15;
  ssOptionsValues4.m_autoCorrDisplay             = true;
  ssOptionsValues4.m_autoCorrWrite               = true;
  ssOptionsValues4.m_meanStaccCompute            = false;
  ssOptionsValues4.m_histCompute                 = true;
  ssOptionsValues4.m_histNumInternalBins         = 250;
  ssOptionsValues4.m_cdfStaccCompute             = false;
  ssOptionsValues4.m_kdeCompute                  = true;
  ssOptionsValues4.m_kdeNumEvalPositions         = 250;
  ssOptionsValues4.m_covMatrixCompute            = true;
  ssOptionsValues4.m_corrMatrixCompute           = true;

  calFpMcOptionsValues = new uqMcOptionsValuesClass(&ssOptionsValues3,&ssOptionsValues4);
  calFpMcOptionsValues->m_dataOutputFileName   = "outputData/tgaCalOutput";
  calFpMcOptionsValues->m_dataOutputAllowedSet.insert(0);
  calFpMcOptionsValues->m_dataOutputAllowedSet.insert(1);

  calFpMcOptionsValues->m_pseqDataOutputFileName   = ".";
  calFpMcOptionsValues->m_pseqDataOutputAllowedSet.insert(0);
  calFpMcOptionsValues->m_pseqDataOutputAllowedSet.insert(1);
  calFpMcOptionsValues->m_pseqComputeStats         = true;

  calFpMcOptionsValues->m_qseqDataInputFileName    = ".";
  calFpMcOptionsValues->m_qseqSize                 = 1048576;
  calFpMcOptionsValues->m_qseqDisplayPeriod        = 20000;
  calFpMcOptionsValues->m_qseqMeasureRunTimes      = true;
  calFpMcOptionsValues->m_qseqDataOutputFileName   = "outputData/file_cal_fp_qoi2";
  calFpMcOptionsValues->m_qseqDataOutputAllowedSet.insert(0);
  calFpMcOptionsValues->m_qseqDataOutputAllowedSet.insert(1);
  calFpMcOptionsValues->m_qseqComputeStats         = true;
#endif
  cycle.calFP().solveWithMonteCarlo(calFpMcOptionsValues); // no extra user entities needed for Monte Carlo algorithm
  delete calFpMcOptionsValues;

  iRC = gettimeofday(&timevalNow, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Ending 'calibration stage' at "        << ctime(&timevalNow.tv_sec)
              << "Total 'calibration stage' run time = " << timevalNow.tv_sec - timevalRef.tv_sec
              << " seconds\n"
              << std::endl;
  }

  //********************************************************
  // Task 3 of 5: validation stage
  //********************************************************

  iRC = gettimeofday(&timevalRef, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Beginning 'validation stage' at " << ctime(&timevalRef.tv_sec)
              << std::endl;
  }

  // Inverse problem: no need to instantiate the prior rv (= posterior rv of calibration inverse problem)

  // Inverse problem: instantiate the likelihood function object (data + routine)
  likelihoodRoutine_DataClass<P_V,P_M> valLikelihoodRoutine_Data(env,
                                                                 "inputData/scenario_100_K_min.dat",
                                                                 NULL,
                                                                 NULL);

  uqGenericScalarFunctionClass<P_V,P_M> valLikelihoodFunctionObj("val_like_",
                                                                 paramDomain,
                                                                 likelihoodRoutine<P_V,P_M>,
                                                                 (void *) &valLikelihoodRoutine_Data,
                                                                 true); // the routine computes [ln(function)]

  // Inverse problem: instantiate it (posterior rv is instantiated internally)
  uqSipOptionsValuesClass* valIpOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  valIpOptionsValues = new uqSipOptionsValuesClass();
  valIpOptionsValues->m_computeSolution      = true;
  valIpOptionsValues->m_dataOutputFileName   = "outputData/tgaValOutput";
  valIpOptionsValues->m_dataOutputAllowedSet.insert(0);
  valIpOptionsValues->m_dataOutputAllowedSet.insert(1);
#endif
  cycle.instantiateValIP(valIpOptionsValues,
                         valLikelihoodFunctionObj);

  // Inverse problem: solve it, that is, set 'pdf' and 'realizer' of the posterior rv
  uqMhOptionsValuesClass* valIpMhOptionsValues = NULL;

  const uqSequentialVectorRealizerClass<P_V,P_M>* tmpRealizer = dynamic_cast< const uqSequentialVectorRealizerClass<P_V,P_M>* >(&(cycle.calIP().postRv().realizer()));
  P_M* valProposalCovMatrix = cycle.calIP().postRv().imageSet().vectorSpace().newProposalMatrix(&tmpRealizer->unifiedSampleVarVector(),  // Use 'realizer()' because the post. rv was computed with Metr. Hast.
                                                                                                &tmpRealizer->unifiedSampleExpVector()); // Use these values as the initial values
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  uqSsOptionsValuesClass ssOptionsValues5;
  uqSsOptionsValuesClass ssOptionsValues6;

  ssOptionsValues5.m_initialDiscardedPortions.resize(9);
  ssOptionsValues5.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues5.m_initialDiscardedPortions[1] = 0.05;
  ssOptionsValues5.m_initialDiscardedPortions[2] = 0.10;
  ssOptionsValues5.m_initialDiscardedPortions[3] = 0.15;
  ssOptionsValues5.m_initialDiscardedPortions[4] = 0.20;
  ssOptionsValues5.m_initialDiscardedPortions[5] = 0.25;
  ssOptionsValues5.m_initialDiscardedPortions[6] = 0.30;
  ssOptionsValues5.m_initialDiscardedPortions[7] = 0.35;
  ssOptionsValues5.m_initialDiscardedPortions[8] = 0.40;
  ssOptionsValues5.m_bmmRun                      = false;
  ssOptionsValues5.m_fftCompute                  = false;
  ssOptionsValues5.m_psdCompute                  = false;
  ssOptionsValues5.m_psdAtZeroCompute            = false;
  ssOptionsValues5.m_gewekeCompute               = true;
  ssOptionsValues5.m_gewekeNaRatio               = .1;
  ssOptionsValues5.m_gewekeNbRatio               = .5;
  ssOptionsValues5.m_gewekeDisplay               = true;
  ssOptionsValues5.m_gewekeWrite                 = true;
  ssOptionsValues5.m_autoCorrComputeViaDef       = false;
  ssOptionsValues5.m_autoCorrComputeViaFft       = true;
  ssOptionsValues5.m_autoCorrSecondLag           = 2;
  ssOptionsValues5.m_autoCorrLagSpacing          = 2;
  ssOptionsValues5.m_autoCorrNumLags             = 15;
  ssOptionsValues5.m_autoCorrDisplay             = true;
  ssOptionsValues5.m_autoCorrWrite               = true;
  ssOptionsValues5.m_meanStaccCompute            = false;
  ssOptionsValues5.m_histCompute                 = false;
  ssOptionsValues5.m_cdfStaccCompute             = false;
  ssOptionsValues5.m_kdeCompute                  = false;
  ssOptionsValues5.m_covMatrixCompute            = true;
  ssOptionsValues5.m_corrMatrixCompute           = true;

  ssOptionsValues6.m_initialDiscardedPortions.resize(1);
  ssOptionsValues6.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues6.m_bmmRun                      = false;
  ssOptionsValues6.m_fftCompute                  = false;
  ssOptionsValues6.m_psdCompute                  = false;
  ssOptionsValues6.m_psdAtZeroCompute            = false;
  ssOptionsValues6.m_gewekeCompute               = false;
  ssOptionsValues6.m_autoCorrComputeViaDef       = false;
  ssOptionsValues6.m_autoCorrComputeViaFft       = true;
  ssOptionsValues6.m_autoCorrSecondLag           = 2;
  ssOptionsValues6.m_autoCorrLagSpacing          = 2;
  ssOptionsValues6.m_autoCorrNumLags             = 15;
  ssOptionsValues6.m_autoCorrDisplay             = true;
  ssOptionsValues6.m_autoCorrWrite               = true;
  ssOptionsValues6.m_meanStaccCompute            = false;
  ssOptionsValues6.m_histCompute                 = true;
  ssOptionsValues6.m_histNumInternalBins         = 250;
  ssOptionsValues6.m_cdfStaccCompute             = false;
  ssOptionsValues6.m_kdeCompute                  = true;
  ssOptionsValues6.m_kdeNumEvalPositions         = 250;
  ssOptionsValues6.m_covMatrixCompute            = true;
  ssOptionsValues6.m_corrMatrixCompute           = true;

  valIpMhOptionsValues = new uqMhOptionsValuesClass(&ssOptionsValues5,&ssOptionsValues6);
  valIpMhOptionsValues->m_dataOutputFileName   = "outputData/tgaValOutput";
  valIpMhOptionsValues->m_dataOutputAllowedSet.insert(0);
  valIpMhOptionsValues->m_dataOutputAllowedSet.insert(1);

  valIpMhOptionsValues->m_rawChainDataInputFileName    = ".";
  valIpMhOptionsValues->m_rawChainSize                 = 1048576;
  valIpMhOptionsValues->m_rawChainGenerateExtra        = false;
  valIpMhOptionsValues->m_rawChainDisplayPeriod        = 20000;
  valIpMhOptionsValues->m_rawChainMeasureRunTimes      = true;
  valIpMhOptionsValues->m_rawChainDataOutputFileName   = "outputData/file_val_ip_raw";
  valIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(0);
  valIpMhOptionsValues->m_rawChainDataOutputAllowedSet.insert(1);
  valIpMhOptionsValues->m_rawChainComputeStats         = true;

  valIpMhOptionsValues->m_displayCandidates         = false;
  valIpMhOptionsValues->m_putOutOfBoundsInChain     = true;
  valIpMhOptionsValues->m_tkUseLocalHessian         = false;
  valIpMhOptionsValues->m_tkUseNewtonComponent      = true;
  valIpMhOptionsValues->m_drMaxNumExtraStages       = 1;
  valIpMhOptionsValues->m_drScalesForExtraStages.resize(1);
  valIpMhOptionsValues->m_drScalesForExtraStages[0] = 5.;
  valIpMhOptionsValues->m_amInitialNonAdaptInterval = 0;
  valIpMhOptionsValues->m_amAdaptInterval           = 100;
  valIpMhOptionsValues->m_amEta                     = 1.92;
  valIpMhOptionsValues->m_amEpsilon                 = 1.e-5;

  valIpMhOptionsValues->m_filteredChainGenerate             = true;
  valIpMhOptionsValues->m_filteredChainDiscardedPortion     = 0.;
  valIpMhOptionsValues->m_filteredChainLag                  = 20;
  valIpMhOptionsValues->m_filteredChainDataOutputFileName   = ".";
  valIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(0);
  valIpMhOptionsValues->m_filteredChainDataOutputAllowedSet.insert(1);
  valIpMhOptionsValues->m_filteredChainComputeStats         = true;
#endif
  cycle.valIP().solveWithBayesMetropolisHastings(valIpMhOptionsValues,
                                                 tmpRealizer->unifiedSampleExpVector(),
                                                 valProposalCovMatrix);
  delete valProposalCovMatrix;
  delete valIpMhOptionsValues;

  // Forward problem: instantiate it (parameter rv = posterior rv of inverse problem; qoi rv is instantiated internally)
  qoiRoutine_DataClass<P_V,P_M,Q_V,Q_M> valQoiRoutine_Data;
  valQoiRoutine_Data.m_beta         = beta_prediction;
  valQoiRoutine_Data.m_criticalMass = criticalMass_prediction;
  valQoiRoutine_Data.m_criticalTime = criticalTime_prediction;

  uqSfpOptionsValuesClass* valFpOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  valFpOptionsValues = new uqSfpOptionsValuesClass();
  valFpOptionsValues->m_computeSolution      = true;
  valFpOptionsValues->m_computeCovariances   = true;
  valFpOptionsValues->m_computeCorrelations  = true;
  valFpOptionsValues->m_dataOutputFileName   = "outputData/tgaValOutput";
  valFpOptionsValues->m_dataOutputAllowedSet.insert(0);
  valFpOptionsValues->m_dataOutputAllowedSet.insert(1);
#endif
  cycle.instantiateValFP(valFpOptionsValues,
                         qoiRoutine<P_V,P_M,Q_V,Q_M>,
                         (void *) &valQoiRoutine_Data);

  // Forward problem: solve it, that is, set 'realizer' and 'cdf' of the qoi rv
  uqMcOptionsValuesClass* valFpMcOptionsValues = NULL;
#ifdef UQ_EXAMPLES_USES_QUESO_INPUT_FILE
#else
  uqSsOptionsValuesClass ssOptionsValues7;
  uqSsOptionsValuesClass ssOptionsValues8;

  ssOptionsValues7.m_initialDiscardedPortions.resize(1);
  ssOptionsValues7.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues7.m_meanStaccCompute            = false;
  ssOptionsValues7.m_histCompute                 = true;
  ssOptionsValues7.m_histNumInternalBins         = 250;
  ssOptionsValues7.m_cdfStaccCompute             = false;
  ssOptionsValues7.m_kdeCompute                  = true;
  ssOptionsValues7.m_kdeNumEvalPositions         = 250;
  ssOptionsValues7.m_covMatrixCompute            = true;
  ssOptionsValues7.m_corrMatrixCompute           = true;

  ssOptionsValues8.m_initialDiscardedPortions.resize(1);
  ssOptionsValues8.m_initialDiscardedPortions[0] = 0.;
  ssOptionsValues8.m_bmmRun                      = false;
  ssOptionsValues8.m_fftCompute                  = false;
  ssOptionsValues8.m_psdCompute                  = false;
  ssOptionsValues8.m_psdAtZeroCompute            = false;
  ssOptionsValues8.m_gewekeCompute               = false;
  ssOptionsValues8.m_autoCorrComputeViaDef       = false;
  ssOptionsValues8.m_autoCorrComputeViaFft       = true;
  ssOptionsValues8.m_autoCorrSecondLag           = 2;
  ssOptionsValues8.m_autoCorrLagSpacing          = 1;
  ssOptionsValues8.m_autoCorrNumLags             = 15;
  ssOptionsValues8.m_autoCorrDisplay             = true;
  ssOptionsValues8.m_autoCorrWrite               = true;
  ssOptionsValues8.m_meanStaccCompute            = false;
  ssOptionsValues8.m_histCompute                 = true;
  ssOptionsValues8.m_histNumInternalBins         = 250;
  ssOptionsValues8.m_cdfStaccCompute             = false;
  ssOptionsValues8.m_kdeCompute                  = true;
  ssOptionsValues8.m_kdeNumEvalPositions         = 250;
  ssOptionsValues8.m_covMatrixCompute            = true;
  ssOptionsValues8.m_corrMatrixCompute           = true;

  valFpMcOptionsValues = new uqMcOptionsValuesClass(&ssOptionsValues7,&ssOptionsValues8);
  valFpMcOptionsValues->m_dataOutputFileName   = "outputData/tgaValOutput";
  valFpMcOptionsValues->m_dataOutputAllowedSet.insert(0);
  valFpMcOptionsValues->m_dataOutputAllowedSet.insert(1);

  valFpMcOptionsValues->m_pseqDataOutputFileName   = ".";
  valFpMcOptionsValues->m_pseqDataOutputAllowedSet.insert(0);
  valFpMcOptionsValues->m_pseqDataOutputAllowedSet.insert(1);
  valFpMcOptionsValues->m_pseqComputeStats         = true;

  valFpMcOptionsValues->m_qseqDataInputFileName    = ".";
  valFpMcOptionsValues->m_qseqSize                 = 1048576;
  valFpMcOptionsValues->m_qseqDisplayPeriod        = 20000;
  valFpMcOptionsValues->m_qseqMeasureRunTimes      = true;
  valFpMcOptionsValues->m_qseqDataOutputFileName   = "outputData/file_val_fp_qoi2";
  valFpMcOptionsValues->m_qseqDataOutputAllowedSet.insert(0);
  valFpMcOptionsValues->m_qseqDataOutputAllowedSet.insert(1);
  valFpMcOptionsValues->m_qseqComputeStats         = true;
#endif
  cycle.valFP().solveWithMonteCarlo(valFpMcOptionsValues); // no extra user entities needed for Monte Carlo algorithm
  delete valFpMcOptionsValues;

  delete valFpOptionsValues;
  delete valIpOptionsValues;
  delete calFpOptionsValues;
  delete calIpOptionsValues;

  iRC = gettimeofday(&timevalNow, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Ending 'validation stage' at "        << ctime(&timevalNow.tv_sec)
              << "Total 'validation stage' run time = " << timevalNow.tv_sec - timevalRef.tv_sec
              << " seconds\n"
              << std::endl;
  }

  //********************************************************
  // Task 4 of 5: comparison stage
  //********************************************************

  iRC = gettimeofday(&timevalRef, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Beginning 'comparison stage' at " << ctime(&timevalRef.tv_sec)
              << std::endl;
  }

  uqAppl_LocalComparisonStage(cycle);
  if (env.numSubEnvironments() > 1) {
    uqAppl_UnifiedComparisonStage(cycle);
  }

  iRC = gettimeofday(&timevalNow, NULL);
  if (env.fullRank() == 0) {
    std::cout << "Ending 'comparison stage' at "        << ctime(&timevalNow.tv_sec)
              << "Total 'comparison stage' run time = " << timevalNow.tv_sec - timevalRef.tv_sec
              << " seconds\n"
              << std::endl;
  }

  //******************************************************
  // Task 5 of 5: release memory before leaving routine.
  //******************************************************

  if (env.fullRank() == 0) {
    std::cout << "Finishing run of 'uqTgaExample' example"
              << std::endl;
  }

  return;
}

//********************************************************
// The 'local comparison stage' of the driving routine "uqAppl()"
//********************************************************
template<class P_V,class P_M,class Q_V,class Q_M>
void 
uqAppl_LocalComparisonStage(uqValidationCycleClass<P_V,P_M,Q_V,Q_M>& cycle)
{
  if (cycle.calFP().computeSolutionFlag() &&
      cycle.valFP().computeSolutionFlag()) {
    Q_V cdfDistancesVec(cycle.calFP().qoiRv().imageSet().vectorSpace().zeroVector());
    Q_V epsilonVec     (cycle.calFP().qoiRv().imageSet().vectorSpace().zeroVector());

    // Epsilon = 0.02
    epsilonVec.cwSet(0.02);
    horizontalDistances(cycle.calFP().qoiRv().unifiedCdf(),
                        cycle.valFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "    << epsilonVec
                                    << ", cdfDistancesVec = " << cdfDistancesVec
                                    << std::endl;
    }

    // Test independence of 'distance' w.r.t. order of cdfs
    horizontalDistances(cycle.valFP().qoiRv().unifiedCdf(),
                        cycle.calFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "                             << epsilonVec
                                    << ", cdfDistancesVec (switched order of cdfs) = " << cdfDistancesVec
                                    << std::endl;
    }

    // Epsilon = 0.04
    epsilonVec.cwSet(0.04);
    horizontalDistances(cycle.calFP().qoiRv().unifiedCdf(),
                        cycle.valFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "    << epsilonVec
                                    << ", cdfDistancesVec = " << cdfDistancesVec
                                    << std::endl;
    }

    // Epsilon = 0.06
    epsilonVec.cwSet(0.06);
    horizontalDistances(cycle.calFP().qoiRv().unifiedCdf(),
                        cycle.valFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "    << epsilonVec
                                    << ", cdfDistancesVec = " << cdfDistancesVec
                                    << std::endl;
    }

    // Epsilon = 0.08
    epsilonVec.cwSet(0.08);
    horizontalDistances(cycle.calFP().qoiRv().unifiedCdf(),
                        cycle.valFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "    << epsilonVec
                                    << ", cdfDistancesVec = " << cdfDistancesVec
                                    << std::endl;
    }

    // Epsilon = 0.10
    epsilonVec.cwSet(0.10);
    horizontalDistances(cycle.calFP().qoiRv().unifiedCdf(),
                        cycle.valFP().qoiRv().unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().subDisplayFile()) {
      *cycle.env().subDisplayFile() << "For epsilonVec = "    << epsilonVec
                                    << ", cdfDistancesVec = " << cdfDistancesVec
                                    << std::endl;
    }
  }

  return;
}

//********************************************************
// The 'unified comparison stage' of the driving routine "uqAppl()"
//********************************************************
template<class P_V,class P_M,class Q_V,class Q_M>
void 
uqAppl_UnifiedComparisonStage(uqValidationCycleClass<P_V,P_M,Q_V,Q_M>& cycle)
{
  if (cycle.calFP().computeSolutionFlag() &&
      cycle.valFP().computeSolutionFlag()) {
    Q_V cdfDistancesVec(cycle.calFP().qoiRv().imageSet().vectorSpace().zeroVector());
    Q_V epsilonVec     (cycle.calFP().qoiRv().imageSet().vectorSpace().zeroVector());

    // Epsilon = 0.02
    epsilonVec.cwSet(0.02);
    horizontalDistances(cycle.calFP().qoiRv_unifiedCdf(),
                        cycle.valFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "           << epsilonVec
                << ", unifiedCdfDistancesVec = " << cdfDistancesVec
                << std::endl;
    }

    // Test independence of 'distance' w.r.t. order of cdfs
    horizontalDistances(cycle.valFP().qoiRv_unifiedCdf(),
                        cycle.calFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "                                    << epsilonVec
                << ", unifiedCdfDistancesVec (switched order of cdfs) = " << cdfDistancesVec
                << std::endl;
    }

    // Epsilon = 0.04
    epsilonVec.cwSet(0.04);
    horizontalDistances(cycle.calFP().qoiRv_unifiedCdf(),
                        cycle.valFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "           << epsilonVec
                << ", unifiedCdfDistancesVec = " << cdfDistancesVec
                << std::endl;
    }

    // Epsilon = 0.06
    epsilonVec.cwSet(0.06);
    horizontalDistances(cycle.calFP().qoiRv_unifiedCdf(),
                        cycle.valFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "           << epsilonVec
                << ", unifiedCdfDistancesVec = " << cdfDistancesVec
                << std::endl;
    }

    // Epsilon = 0.08
    epsilonVec.cwSet(0.08);
    horizontalDistances(cycle.calFP().qoiRv_unifiedCdf(),
                        cycle.valFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "           << epsilonVec
                << ", unifiedCdfDistancesVec = " << cdfDistancesVec
                << std::endl;
    }

    // Epsilon = 0.10
    epsilonVec.cwSet(0.10);
    horizontalDistances(cycle.calFP().qoiRv_unifiedCdf(),
                        cycle.valFP().qoiRv_unifiedCdf(),
                        epsilonVec,
                        cdfDistancesVec);
    if (cycle.env().fullRank() == 0) {
      std::cout << "For epsilonVec = "           << epsilonVec
                << ", unifiedCdfDistancesVec = " << cdfDistancesVec
                << std::endl;
    }
  }

  return;
}
#endif // __EX_TGA_VALIDATION_CYCLE_APPL_H__
