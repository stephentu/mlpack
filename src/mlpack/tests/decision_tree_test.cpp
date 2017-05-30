/**
 * @file decision_tree_test.cpp
 * @author Ryan Curtin
 *
 * Tests for the DecisionTree class and related classes.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#include <mlpack/core.hpp>
#include <mlpack/methods/decision_tree/decision_tree.hpp>
#include <mlpack/methods/decision_tree/information_gain.hpp>
#include <mlpack/methods/decision_tree/gini_gain.hpp>

#include <boost/test/unit_test.hpp>
#include "test_tools.hpp"
#include "serialization.hpp"

using namespace mlpack;
using namespace mlpack::tree;
using namespace mlpack::distribution;

/**
 * Create a mock categorical dataset for testing.
 */
void MockCategoricalData(arma::mat& d,
                         arma::Row<size_t>& l,
                         data::DatasetInfo& datasetInfo)
{
  // We'll build a spiral dataset plus two noisy categorical features.  We need
  // to build the distributions for the categorical features (they'll be
  // discrete distributions).
  DiscreteDistribution c1[5];
  // The distribution will be automatically normalized.
  for (size_t i = 0; i < 5; ++i)
  {
    std::vector<arma::vec> probs;
    probs.push_back(arma::vec(4, arma::fill::randu));
    c1[i] = DiscreteDistribution(probs);
  }

  DiscreteDistribution c2[5];
  for (size_t i = 0; i < 5; ++i)
  {
    std::vector<arma::vec> probs;
    probs.push_back(arma::vec(2, arma::fill::randu));
    c2[i] = DiscreteDistribution(probs);
  }

  arma::mat spiralDataset(4, 4000);
  arma::Row<size_t> labels(4000);
  for (size_t i = 0; i < 4000; ++i)
  {
    // One circle every 2000 samples.  Plus some noise.
    const double magnitude = 2.0 + (double(i) / 200.0) +
        0.5 * mlpack::math::Random();
    const double angle = (i % 200) * (2 * M_PI) + mlpack::math::Random();

    const double x = magnitude * cos(angle);
    const double y = magnitude * sin(angle);

    spiralDataset(0, i) = x;
    spiralDataset(1, i) = y;

    // Set categorical features c1 and c2.
    if (i < 800)
    {
      spiralDataset(2, i) = c1[1].Random()[0];
      spiralDataset(3, i) = c2[1].Random()[0];
      labels[i] = 1;
    }
    else if (i < 1600)
    {
      spiralDataset(2, i) = c1[3].Random()[0];
      spiralDataset(3, i) = c2[3].Random()[0];
      labels[i] = 3;
    }
    else if (i < 2400)
    {
      spiralDataset(2, i) = c1[2].Random()[0];
      spiralDataset(3, i) = c2[2].Random()[0];
      labels[i] = 2;
    }
    else if (i < 3200)
    {
      spiralDataset(2, i) = c1[0].Random()[0];
      spiralDataset(3, i) = c2[0].Random()[0];
      labels[i] = 0;
    }
    else
    {
      spiralDataset(2, i) = c1[4].Random()[0];
      spiralDataset(3, i) = c2[4].Random()[0];
      labels[i] = 4;
    }
  }

  // Now create the dataset info.
  datasetInfo = data::DatasetInfo(4);
  datasetInfo.Type(2) = data::Datatype::categorical;
  datasetInfo.Type(3) = data::Datatype::categorical;
  // Set mappings.
  datasetInfo.MapString<double>("0", 2);
  datasetInfo.MapString<double>("1", 2);
  datasetInfo.MapString<double>("2", 2);
  datasetInfo.MapString<double>("3", 2);
  datasetInfo.MapString<double>("0", 3);
  datasetInfo.MapString<double>("1", 3);

  // Now shuffle the dataset.
  arma::uvec indices = arma::shuffle(arma::linspace<arma::uvec>(0, 3999,
      4000));
  d = arma::mat(4, 4000);
  l = arma::Row<size_t>(4000);
  for (size_t i = 0; i < 4000; ++i)
  {
    d.col(i) = spiralDataset.col(indices[i]);
    l[i] = labels[indices[i]];
  }
}

BOOST_AUTO_TEST_SUITE(DecisionTreeTest);

/**
 * Make sure the Gini gain is zero when the labels are perfect.
 */
BOOST_AUTO_TEST_CASE(GiniGainPerfectTest)
{
  arma::rowvec weights(10, arma::fill::ones);
  arma::Row<size_t> labels;
  labels.zeros(10);

  // Test that it's perfect regardless of number of classes.
  for (size_t c = 1; c < 10; ++c)
    BOOST_REQUIRE_SMALL(GiniGain::Evaluate<false>(labels, c, weights), 1e-5);
}

/**
 * Make sure the Gini gain is -0.5 when the class split between two classes
 * is even.
 */
BOOST_AUTO_TEST_CASE(GiniGainEvenSplitTest)
{
  arma::rowvec weights = arma::ones<arma::rowvec>(10);
  arma::Row<size_t> labels(10);
  for (size_t i = 0; i < 5; ++i)
    labels[i] = 0;
  for (size_t i = 5; i < 10; ++i)
    labels[i] = 1;

  // Test that it's -0.5 regardless of the number of classes.
  for (size_t c = 2; c < 10; ++c)
  {
    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<false>(labels, c, weights), -0.5, 1e-5);
    double weightedGain = GiniGain::Evaluate<true>(labels, c, weights);

    // The weighted gain should stay the same with unweight one
    BOOST_REQUIRE_EQUAL(GiniGain::Evaluate<false>(labels, c, weights), weightedGain);
  }
}

/**
 * The Gini gain of an empty vector is 0.
 */
BOOST_AUTO_TEST_CASE(GiniGainEmptyTest)
{
  arma::rowvec weights = arma::ones<arma::rowvec>(10);
  // Test across some numbers of classes.
  arma::Row<size_t> labels;
  for (size_t c = 1; c < 10; ++c)
    BOOST_REQUIRE_SMALL(GiniGain::Evaluate<false>(labels, c, weights), 1e-5);
  
  for (size_t c = 1; c < 10; ++c)
    BOOST_REQUIRE_SMALL(GiniGain::Evaluate<true>(labels, c, weights), 1e-5);
}

/**
 * The Gini gain is -(1 - 1/k) for k classes evenly split.
 */
BOOST_AUTO_TEST_CASE(GiniGainEvenSplitManyClassTest)
{
  // Try with many different classes.
  for (size_t c = 2; c < 30; ++c)
  {
    arma::Row<size_t> labels(c);
    arma::rowvec weights(c);
    for (size_t i = 0; i < c; ++i)
    {
      labels[i] = i;
      weights[i] = 1;
    }

    // Calculate Gini gain and make sure it is correct.
    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<false>(labels, c, weights),
        -(1.0 - 1.0 / c), 1e-5);
    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<true>(labels, c, weights),
        -(1.0 - 1.0 / c), 1e-5);
  }
}

/**
 * The Gini gain should not be sensitive to the number of points.
 */
BOOST_AUTO_TEST_CASE(GiniGainManyPoints)
{
  for (size_t i = 1; i < 20; ++i)
  {
    const size_t numPoints = 100 * i;
    arma::rowvec weights(numPoints);
    weights.ones();
    arma::Row<size_t> labels(numPoints);
    for (size_t j = 0; j < numPoints / 2; ++j)
      labels[j] = 0;
    for (size_t j = numPoints / 2; j < numPoints; ++j)
      labels[j] = 1;

    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<false>(labels, 2, weights), -0.5,
        1e-5);
    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<true>(labels, 2, weights), -0.5,
        1e-5);
  }
}

/** 
 * To make sure the Gini gain can been cacluate proporately with weight.
 */
 BOOST_AUTO_TEST_CASE(GiniGainWithWeight)
 {
    arma::Row<size_t> labels(10);
    arma::rowvec weights(10);
    for (size_t i = 0; i < 5; ++i)
    {
      labels[i] = 0;
      weights[i] = 0.3;
    }
    for (size_t i = 5; i < 10; ++i)
    {
      labels[i] = 1;
      weights[i] = 0.7;
    }

    BOOST_REQUIRE_CLOSE(GiniGain::Evaluate<true>(labels, 2, weights), -0.42, 1e-5);
 }

/**
 * The information gain should be zero when the labels are perfect.
 */
BOOST_AUTO_TEST_CASE(InformationGainPerfectTest)
{
  arma::rowvec weights;
  arma::Row<size_t> labels;
  labels.zeros(10);

  // Test that it's perfect regardless of number of classes.
  for (size_t c = 1; c < 10; ++c)
    BOOST_REQUIRE_SMALL(InformationGain::Evaluate<false>(labels, c, weights), 1e-5);
}

/**
 * If we have an even split, the information gain should be -1.
 */
BOOST_AUTO_TEST_CASE(InformationGainEvenSplitTest)
{
  arma::Row<size_t> labels(10);
  arma::rowvec weights(10);
  weights.ones();
  for (size_t i = 0; i < 5; ++i)
    labels[i] = 0;
  for (size_t i = 5; i < 10; ++i)
    labels[i] = 1;

  // Test that it's -1 regardless of the number of classes.
  for (size_t c = 2; c < 10; ++c)
  {
    // Weighted and unweighted result should be the same.
    BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<false>(labels, c, weights),
        -1.0, 1e-5);
    BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<true>(labels, c, weights),
        -1.0, 1e-5);
  }
}

/**
 * The information gain of an empty vector is 0.
 */
BOOST_AUTO_TEST_CASE(InformationGainEmptyTest)
{
  arma::Row<size_t> labels;
  arma::rowvec weights = arma::ones<arma::rowvec>(10);
  for (size_t c = 1; c < 10; ++c)
  {
    BOOST_REQUIRE_SMALL(InformationGain::Evaluate<false>(labels, c, weights),
        1e-5);
    BOOST_REQUIRE_SMALL(InformationGain::Evaluate<true>(labels, c, weights),
        1e-5);
  }
}

/**
 * The information gain is log2(1/k) when splitting equal classes.
 */
BOOST_AUTO_TEST_CASE(InformationGainEvenSplitManyClassTest)
{
  arma::rowvec weights;
  // Try with many different numbers of classes.
  for (size_t c = 2; c < 30; ++c)
  {
    arma::Row<size_t> labels(c);
    for (size_t i = 0; i < c; ++i)
      labels[i] = i;

    // Calculate information gain and make sure it is correct.
    BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<false>(labels, c, weights),
        std::log2(1.0 / c), 1e-5);
  }
}

/**
 * Test the information gain with weighted labels
 */
BOOST_AUTO_TEST_CASE(InformationWithWeight)
{
  arma::Row<size_t> labels(10);
  arma::rowvec weights("1 1 1 1 1 0 0 0 0 0");
  for (size_t i = 0; i < 5; ++i)
    labels[i] = 0;
  for (size_t i = 5; i < 10; ++i)
    labels[i] = 1;

  // Zero is not a good result as gain, but we just need to prove cacluation works.
  BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<true>(labels, 2, weights), 0, 1e-5);
  
}


/**
 * The information gain should not be sensitive to the number of points.
 */
BOOST_AUTO_TEST_CASE(InformationGainManyPoints)
{
  for (size_t i = 1; i < 20; ++i)
  {
    const size_t numPoints = 100 * i;
    arma::Row<size_t> labels(numPoints);
    arma::rowvec weights = arma::ones<arma::rowvec>(numPoints);
    for (size_t j = 0; j < numPoints / 2; ++j)
      labels[j] = 0;
    for (size_t j = numPoints / 2; j < numPoints; ++j)
      labels[j] = 1;

    BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<false>(labels, 2, weights),
        -1.0, 1e-5);
    // It should make no difference between a weighted and unweighted
    // calculation.
    BOOST_REQUIRE_CLOSE(InformationGain::Evaluate<true>(labels, 2, weights),
        -1.0, 1e-5);
  }
}

/**
 * Check that the BestBinaryNumericSplit will split on an obviously splittable
 * dimension.
 */
BOOST_AUTO_TEST_CASE(BestBinaryNumericSplitSimpleSplitTest)
{
  arma::vec values("0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0");
  arma::Row<size_t> labels("0 0 0 0 0 1 1 1 1 1 1");
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  arma::vec classProbabilities;
  BestBinaryNumericSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 2, weights);
  const double gain = BestBinaryNumericSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, labels, 2, weights, 3, classProbabilities, aux);
  const double weightedGain =
      BestBinaryNumericSplit<GiniGain>::SplitIfBetter<true>(bestGain, values,
      labels, 2, weights, 3, classProbabilities, aux);

  // Make sure that a split was made.
  BOOST_REQUIRE_GT(gain, bestGain);

  // Make sure weight works and make no different with no weighted one
  BOOST_REQUIRE_EQUAL(gain, weightedGain);

  // The split is perfect, so we should be able to accomplish a gain of 0.
  BOOST_REQUIRE_SMALL(gain, 1e-5);

  // The class probabilities, for this split, hold the splitting point, which
  // should be between 4 and 5.
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 1);
  BOOST_REQUIRE_GT(classProbabilities[0], 0.4);
  BOOST_REQUIRE_LT(classProbabilities[0], 0.5);
}

/**
 * Check that the BestBinaryNumericSplit won't split if not enough points are
 * given.
 */
BOOST_AUTO_TEST_CASE(BestBinaryNumericSplitMinSamplesTest)
{
  arma::vec values("0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0");
  arma::Row<size_t> labels("0 0 0 0 0 1 1 1 1 1 1");
  arma::rowvec weights(labels.n_elem);

  arma::vec classProbabilities;
  BestBinaryNumericSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 2, weights);
  const double gain = BestBinaryNumericSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, labels, 2, weights, 8, classProbabilities, aux);
  // This should make no difference because it won't split at all. 
  const double weightedGain =
      BestBinaryNumericSplit<GiniGain>::SplitIfBetter<true>(bestGain, values,
      labels, 2, weights, 8, classProbabilities, aux);

  // Make sure that no split was made.
  BOOST_REQUIRE_EQUAL(gain, bestGain);
  BOOST_REQUIRE_EQUAL(gain, weightedGain);
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 0);
}

/**
 * Check that the BestBinaryNumericSplit doesn't split a dimension that gives no
 * gain.
 */
BOOST_AUTO_TEST_CASE(BestBinaryNumericSplitNoGainTest)
{
  arma::vec values(100);
  arma::Row<size_t> labels(100);
  arma::rowvec weights;
  for (size_t i = 0; i < 100; i += 2)
  {
    values[i] = i;
    labels[i] = 0;
    values[i + 1] = i;
    labels[i + 1] = 1;
  }

  arma::vec classProbabilities;
  BestBinaryNumericSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 2, weights);
  const double gain = BestBinaryNumericSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, labels, 2, weights, 10, classProbabilities, aux);

  // Make sure there was no split.
  BOOST_REQUIRE_EQUAL(gain, bestGain);
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 0);
}

/**
 * Check that the AllCategoricalSplit will split when the split is obviously
 * better.
 */
BOOST_AUTO_TEST_CASE(AllCategoricalSplitSimpleSplitTest)
{
  arma::vec values("0 0 0 1 1 1 2 2 2 3 3 3");
  arma::Row<size_t> labels("0 0 0 2 2 2 1 1 1 2 2 2");
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  arma::vec classProbabilities;
  AllCategoricalSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 3, weights);
  const double gain = AllCategoricalSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, 4, labels, 3, weights, 3, classProbabilities, aux);
  const double weightedGain =
      AllCategoricalSplit<GiniGain>::SplitIfBetter<true>(bestGain, values, 4,
      labels, 3, weights, 3, classProbabilities, aux);

  // Make sure that a split was made.
  BOOST_REQUIRE_GT(gain, bestGain);

  // Since the split is perfect, make sure the new gain is 0.
  BOOST_REQUIRE_SMALL(gain, 1e-5);

  BOOST_REQUIRE_EQUAL(gain, weightedGain);

  // Make sure the class probabilities now hold the number of children.
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 1);
  BOOST_REQUIRE_EQUAL((size_t) classProbabilities[0], 4);
}

/**
 * Make sure that AllCategoricalSplit respects the minimum number of samples
 * required to split.
 */
BOOST_AUTO_TEST_CASE(AllCategoricalSplitMinSamplesTest)
{
  arma::vec values("0 0 0 1 1 1 2 2 2 3 3 3");
  arma::Row<size_t> labels("0 0 0 2 2 2 1 1 1 2 2 2");
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  arma::vec classProbabilities;
  AllCategoricalSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 3, weights);
  const double gain = AllCategoricalSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, 4, labels, 3, weights, 4, classProbabilities, aux);

  // Make sure it's not split.
  BOOST_REQUIRE_EQUAL(gain, bestGain);
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 0);
}

/**
 * Check that no split is made when it doesn't get us anything.
 */
BOOST_AUTO_TEST_CASE(AllCategoricalSplitNoGainTest)
{
  arma::vec values(300);
  arma::Row<size_t> labels(300);
  arma::rowvec weights = arma::ones<arma::rowvec>(300);

  for (size_t i = 0; i < 300; i += 3)
  {
    values[i] = (i / 3) % 10;
    labels[i] = 0;
    values[i + 1] = (i / 3) % 10;
    labels[i + 1] = 1;
    values[i + 2] = (i / 3) % 10;
    labels[i + 2] = 2;
  }

  arma::vec classProbabilities;
  AllCategoricalSplit<GiniGain>::template AuxiliarySplitInfo<double> aux;

  // Call the method to do the splitting.
  const double bestGain = GiniGain::Evaluate<false>(labels, 3, weights);
  const double gain = AllCategoricalSplit<GiniGain>::SplitIfBetter<false>(
      bestGain, values, 10, labels, 3, weights, 10, classProbabilities, aux);
  const double weightedGain =
      AllCategoricalSplit<GiniGain>::SplitIfBetter<true>(bestGain, values, 10,
      labels, 3, weights, 10, classProbabilities, aux);

  // Make sure that there was no split.
  BOOST_REQUIRE_EQUAL(gain, bestGain);
  BOOST_REQUIRE_EQUAL(gain, weightedGain);
  BOOST_REQUIRE_EQUAL(classProbabilities.n_elem, 0);
}

/**
 * A basic construction of the decision tree---ensure that we can create the
 * tree and that it split at least once.
 */
BOOST_AUTO_TEST_CASE(BasicConstructionTest)
{
  arma::mat dataset(10, 1000, arma::fill::randu);
  arma::Row<size_t> labels(1000);

  for (size_t i = 0; i < 1000; ++i)
    labels[i] = i % 3; // 3 classes.

  // Use default parameters.
  DecisionTree<> d(dataset, labels, 3, 50);

  // Now require that we have some children.
  BOOST_REQUIRE_GT(d.NumChildren(), 0);
}

/**
 * Construct a tree with weighted labels.
 */
BOOST_AUTO_TEST_CASE(BasicConstructionTestWithWeight)
{
  arma::mat dataset(10, 1000, arma::fill::randu);
  arma::Row<size_t> labels(1000);
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  for (size_t i = 0; i < 1000; ++i)
    labels[i] = i % 3; // 3 classes.

  // Use default parameters.
  DecisionTree<> wd(dataset, labels, 3, weights, 50);
  DecisionTree<> d(dataset, labels, 3, 50);

  // Now require that we have some children.
  BOOST_REQUIRE_GT(wd.NumChildren(), 0);
  BOOST_REQUIRE_EQUAL(wd.NumChildren(), d.NumChildren());
}

/**
 * Construct the decision tree on numeric data only and see that we can fit it
 * exactly and achieve perfect performance on the training set.
 */
BOOST_AUTO_TEST_CASE(PerfectTrainingSet)
{
  // Completely random dataset with no structure.
  arma::mat dataset(10, 1000, arma::fill::randu);
  arma::Row<size_t> labels(1000);
  for (size_t i = 0; i < 1000; ++i)
    labels[i] = i % 3; // 3 classes.
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  DecisionTree<> d(dataset, labels, 3, 1); // Minimum leaf size of 1.

  // Make sure that we can get perfect accuracy on the training set.
  for (size_t i = 0; i < 1000; ++i)
  {
    size_t prediction;
    arma::vec probabilities;
    d.Classify(dataset.col(i), prediction, probabilities);

    BOOST_REQUIRE_EQUAL(prediction, labels[i]);
    BOOST_REQUIRE_EQUAL(probabilities.n_elem, 3);
    for (size_t j = 0; j < 3; ++j)
    {
      if (labels[i] == j)
        BOOST_REQUIRE_CLOSE(probabilities[j], 1.0, 1e-5);
      else
        BOOST_REQUIRE_SMALL(probabilities[j], 1e-5);
    }
  }
}

/**
 * onstruct the decision tree with weighted labels
 */
BOOST_AUTO_TEST_CASE(PerfectTrainingSetWithWeight)
{
  // Completely random dataset with no structure.
  arma::mat dataset(10, 1000, arma::fill::randu);
  arma::Row<size_t> labels(1000);
  for (size_t i = 0; i < 1000; ++i)
    labels[i] = i % 3; // 3 classes.
  arma::rowvec weights(labels.n_elem);
  weights.ones();

  DecisionTree<> d(dataset, labels, 3, weights, 1); // Minimum leaf size of 1.

  // This part of code is dupliacte with no weighted one.
  for (size_t i = 0; i < 1000; ++i)
  {
    size_t prediction;
    arma::vec probabilities;
    d.Classify(dataset.col(i), prediction, probabilities);

    BOOST_REQUIRE_EQUAL(prediction, labels[i]);
    BOOST_REQUIRE_EQUAL(probabilities.n_elem, 3);
    for (size_t j = 0; j < 3; ++j)
    {
      if (labels[i] == j)
        BOOST_REQUIRE_CLOSE(probabilities[j], 1.0, 1e-5);
      else
        BOOST_REQUIRE_SMALL(probabilities[j], 1e-5);
    }
  }
}


/**
 * Make sure class probabilities are computed correctly in the root node.
 */
BOOST_AUTO_TEST_CASE(ClassProbabilityTest)
{
  arma::mat dataset(5, 100, arma::fill::randu);
  arma::Row<size_t> labels(100);
  for (size_t i = 0; i < 100; i += 2)
  {
    labels[i] = 0;
    labels[i + 1] = 1;
  }

  // Create a decision tree that can't split.
  DecisionTree<> d(dataset, labels, 2, 1000);

  BOOST_REQUIRE_EQUAL(d.NumChildren(), 0);

  // Estimate a point's probabilities.
  arma::vec probabilities;
  size_t prediction;
  d.Classify(dataset.col(0), prediction, probabilities);

  BOOST_REQUIRE_EQUAL(probabilities.n_elem, 2);
  BOOST_REQUIRE_CLOSE(probabilities[0], 0.5, 1e-5);
  BOOST_REQUIRE_CLOSE(probabilities[1], 0.5, 1e-5);
}

/**
 * Test that the decision tree generalizes reasonably.
 */
BOOST_AUTO_TEST_CASE(SimpleGeneralizationTest)
{
  arma::mat inputData;
  if (!data::Load("vc2.csv", inputData))
    BOOST_FAIL("Cannot load test dataset vc2.csv!");

  arma::Row<size_t> labels;
  if (!data::Load("vc2_labels.txt", labels))
    BOOST_FAIL("Cannot load labels for vc2_labels.txt");

  // Initialize an all-ones weight matrix.
  arma::rowvec weights(labels.n_cols, arma::fill::ones);

  // Build decision tree.
  DecisionTree<> d(inputData, labels, 3, 10); // Leaf size of 10.
  DecisionTree<> wd(inputData, labels, 3, weights, 10); // Leaf size of 10.

  // Load testing data.
  arma::mat testData;
  if (!data::Load("vc2_test.csv", testData))
    BOOST_FAIL("Cannot load test dataset vc2_test.csv!");

  arma::Mat<size_t> trueTestLabels;
  if (!data::Load("vc2_test_labels.txt", trueTestLabels))
    BOOST_FAIL("Cannot load labels for vc2_test_labels.txt");

  // Get the predicted test labels.
  arma::Row<size_t> predictions;
  d.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);

  // Figure out the accuracy.
  double correct = 0.0;
  for (size_t i = 0; i < predictions.n_elem; ++i)
    if (predictions[i] == trueTestLabels[i])
      ++correct;
  correct /= predictions.n_elem;

  BOOST_REQUIRE_GT(correct, 0.75);

  // reset the prediction
  predictions.zeros();
  wd.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);

  // Figure out the accuracy.
  double wdcorrect = 0.0;
  for (size_t i = 0; i < predictions.n_elem; ++i)
    if (predictions[i] == trueTestLabels[i])
      ++wdcorrect;
  wdcorrect /= predictions.n_elem;

  BOOST_REQUIRE_GT(wdcorrect, 0.75);
}

/**
 * Test that we can build a decision tree on a simple categorical dataset.
 */
BOOST_AUTO_TEST_CASE(CategoricalBuildTest)
{
  arma::mat d;
  arma::Row<size_t> l;
  data::DatasetInfo di;
  MockCategoricalData(d, l, di);
  
  // Split into a training set and a test set.
  arma::mat trainingData = d.cols(0, 1999);
  arma::mat testData = d.cols(2000, 3999);
  arma::Row<size_t> trainingLabels = l.subvec(0, 1999);
  arma::Row<size_t> testLabels = l.subvec(2000, 3999);

  // Build the tree.
  DecisionTree<> tree(trainingData, di, trainingLabels, 5, 10);

  // Now evaluate the accuracy of the tree.
  arma::Row<size_t> predictions;
  tree.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);
  size_t correct = 0;
  for (size_t i = 0; i < testData.n_cols; ++i)
    if (testLabels[i] == predictions[i])
      ++correct;

  // Make sure we got at least 70% accuracy.
  const double correctPct = double(correct) / double(testData.n_cols);
  BOOST_REQUIRE_GT(correctPct, 0.70);
}

/**
 * Test that we can build a decision tree with weights on a simple categorical
 * dataset.
 */
BOOST_AUTO_TEST_CASE(CategoricalBuildTestWithWeight)
{
  arma::mat d;
  arma::Row<size_t> l;
  data::DatasetInfo di;
  MockCategoricalData(d, l, di);

  // Split into a training set and a test set.
  arma::mat trainingData = d.cols(0, 1999);
  arma::mat testData = d.cols(2000, 3999);
  arma::Row<size_t> trainingLabels = l.subvec(0, 1999);
  arma::Row<size_t> testLabels = l.subvec(2000, 3999);

  arma::Row<double> weights = arma::ones<arma::Row<double>>(
      trainingLabels.n_elem);

  // Build the tree.
  DecisionTree<> tree(trainingData, di, trainingLabels, 5, weights, 10);

  // Now evaluate the accuracy of the tree.
  arma::Row<size_t> predictions;
  tree.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);
  size_t correct = 0;
  for (size_t i = 0; i < testData.n_cols; ++i)
    if (testLabels[i] == predictions[i])
      ++correct;

  // Make sure we got at least 70% accuracy.
  const double correctPct = double(correct) / double(testData.n_cols);
  BOOST_REQUIRE_GT(correctPct, 0.70);
}

/**
 * Make sure that when we ask for a decision stump, we get one.
 */
BOOST_AUTO_TEST_CASE(DecisionStumpTest)
{
  // Use a random dataset.
  arma::mat dataset(10, 1000, arma::fill::randu);
  arma::Row<size_t> labels(1000);
  for (size_t i = 0; i < 1000; ++i)
    labels[i] = i % 3; // 3 classes.

  // Build a decision stump.
  DecisionTree<GiniGain, BestBinaryNumericSplit, AllCategoricalSplit, double,
      true> stump(dataset, labels, 3, 1);

  // Check that it has children.
  BOOST_REQUIRE_EQUAL(stump.NumChildren(), 2);
  // Check that its children doesn't have children.
  BOOST_REQUIRE_EQUAL(stump.Child(0).NumChildren(), 0);
  BOOST_REQUIRE_EQUAL(stump.Child(1).NumChildren(), 0);
}

/**
 * Test that we can build a decision tree using weighted data (where the
 * low-weighted data is random noise), and that the tree still builds correctly
 * enough to get good results.
 */
BOOST_AUTO_TEST_CASE(WeightedDecisionTreeTest)
{
  arma::mat dataset;
  arma::Row<size_t> labels;
  data::Load("vc2.csv", dataset);
  data::Load("vc2_labels.txt", labels);

  // Add some noise.
  arma::mat noise(dataset.n_rows, 1000, arma::fill::randu);
  arma::Row<size_t> noiseLabels(1000);
  for (size_t i = 0; i < noiseLabels.n_elem; ++i)
    noiseLabels[i] = math::RandInt(3); // Random label.

  // Concatenate data matrices.
  arma::mat data = arma::join_rows(dataset, noise);
  arma::Row<size_t> fullLabels = arma::join_rows(labels, noiseLabels);

  // Now set weights.
  arma::rowvec weights(dataset.n_cols + 1000);
  for (size_t i = 0; i < dataset.n_cols; ++i)
    weights[i] = math::Random(0.9, 1.0);
  for (size_t i = dataset.n_cols; i < dataset.n_cols + 1000; ++i)
    weights[i] = math::Random(0.0, 0.01); // Low weights for false points.

  // Now build the decision tree.  I think the syntax is right here.
  DecisionTree<> d(data, fullLabels, 3, weights, 10);

  // Now we can check that we get good performance on the VC2 test set.
  arma::mat testData;
  arma::Row<size_t> testLabels;
  data::Load("vc2_test.csv", testData);
  data::Load("vc2_test_labels.txt", testLabels);

  arma::Row<size_t> predictions;
  d.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);

  // Figure out the accuracy.
  double correct = 0.0;
  for (size_t i = 0; i < predictions.n_elem; ++i)
    if (predictions[i] == testLabels[i])
      ++correct;
  correct /= predictions.n_elem;

  BOOST_REQUIRE_GT(correct, 0.75);
}
/**
 * Test that we can build a decision tree on a simple categorical dataset using
 * weights, with low-weight noise added.
 */
BOOST_AUTO_TEST_CASE(CategoricalWeightedBuildTest)
{
  arma::mat d;
  arma::Row<size_t> l;
  data::DatasetInfo di;
  MockCategoricalData(d, l, di);

  // Split into a training set and a test set.
  arma::mat trainingData = d.cols(0, 1999);
  arma::mat testData = d.cols(2000, 3999);
  arma::Row<size_t> trainingLabels = l.subvec(0, 1999);
  arma::Row<size_t> testLabels = l.subvec(2000, 3999);

  // Now create random points.
  arma::mat randomNoise(4, 2000);
  arma::Row<size_t> randomLabels(2000);
  for (size_t i = 0; i < 2000; ++i)
  {
    randomNoise(0, i) = math::Random();
    randomNoise(1, i) = math::Random();
    randomNoise(2, i) = math::RandInt(4);
    randomNoise(3, i) = math::RandInt(2);
    randomLabels[i] = math::RandInt(5);
  }

  // Generate weights.
  arma::rowvec weights(4000);
  for (size_t i = 0; i < 2000; ++i)
    weights[i] = math::Random(0.9, 1.0);
  for (size_t i = 2000; i < 4000; ++i)
    weights[i] = math::Random(0.0, 0.001);

  arma::mat fullData = arma::join_rows(trainingData, randomNoise);
  arma::Row<size_t> fullLabels = arma::join_rows(trainingLabels, randomLabels);

  // Build the tree.
  DecisionTree<> tree(fullData, di, fullLabels, 5, weights, 10);

  // Now evaluate the accuracy of the tree.
  arma::Row<size_t> predictions;
  tree.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);
  size_t correct = 0;
  for (size_t i = 0; i < testData.n_cols; ++i)
    if (testLabels[i] == predictions[i])
      ++correct;

  // Make sure we got at least 70% accuracy.
  const double correctPct = double(correct) / double(testData.n_cols);
  BOOST_REQUIRE_GT(correctPct, 0.70);
}

/**
 * Test that we can build a decision tree using weighted data (where the
 * low-weighted data is random noise) with information gain, and that the tree
 * still builds correctly enough to get good results.
 */
BOOST_AUTO_TEST_CASE(WeightedDecisionTreeInformationGainTest)
{
  arma::mat dataset;
  arma::Row<size_t> labels;
  data::Load("vc2.csv", dataset);
  data::Load("vc2_labels.txt", labels);

  // Add some noise.
  arma::mat noise(dataset.n_rows, 1000, arma::fill::randu);
  arma::Row<size_t> noiseLabels(1000);
  for (size_t i = 0; i < noiseLabels.n_elem; ++i)
    noiseLabels[i] = math::RandInt(3); // Random label.

  // Concatenate data matrices.
  arma::mat data = arma::join_rows(dataset, noise);
  arma::Row<size_t> fullLabels = arma::join_rows(labels, noiseLabels);

  // Now set weights.
  arma::rowvec weights(dataset.n_cols + 1000);
  for (size_t i = 0; i < dataset.n_cols; ++i)
    weights[i] = math::Random(0.9, 1.0);
  for (size_t i = dataset.n_cols; i < dataset.n_cols + 1000; ++i)
    weights[i] = math::Random(0.0, 0.01); // Low weights for false points.

  // Now build the decision tree.  I think the syntax is right here.
  DecisionTree<InformationGain> d(data, fullLabels, 3, weights, 10);

  // Now we can check that we get good performance on the VC2 test set.
  arma::mat testData;
  arma::Row<size_t> testLabels;
  data::Load("vc2_test.csv", testData);
  data::Load("vc2_test_labels.txt", testLabels);

  arma::Row<size_t> predictions;
  d.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);

  // Figure out the accuracy.
  double correct = 0.0;
  for (size_t i = 0; i < predictions.n_elem; ++i)
    if (predictions[i] == testLabels[i])
      ++correct;
  correct /= predictions.n_elem;

  BOOST_REQUIRE_GT(correct, 0.75);
}
/**
 * Test that we can build a decision tree using information gain on a simple
 * categorical dataset using weights, with low-weight noise added.
 */
BOOST_AUTO_TEST_CASE(CategoricalInformationGainWeightedBuildTest)
{
  arma::mat d;
  arma::Row<size_t> l;
  data::DatasetInfo di;
  MockCategoricalData(d, l, di);

  // Split into a training set and a test set.
  arma::mat trainingData = d.cols(0, 1999);
  arma::mat testData = d.cols(2000, 3999);
  arma::Row<size_t> trainingLabels = l.subvec(0, 1999);
  arma::Row<size_t> testLabels = l.subvec(2000, 3999);

  // Now create random points.
  arma::mat randomNoise(4, 2000);
  arma::Row<size_t> randomLabels(2000);
  for (size_t i = 0; i < 2000; ++i)
  {
    randomNoise(0, i) = math::Random();
    randomNoise(1, i) = math::Random();
    randomNoise(2, i) = math::RandInt(4);
    randomNoise(3, i) = math::RandInt(2);
    randomLabels[i] = math::RandInt(5);
  }

  // Generate weights.
  arma::rowvec weights(4000);
  for (size_t i = 0; i < 2000; ++i)
    weights[i] = math::Random(0.9, 1.0);
  for (size_t i = 2000; i < 4000; ++i)
    weights[i] = math::Random(0.0, 0.001);

  arma::mat fullData = arma::join_rows(trainingData, randomNoise);
  arma::Row<size_t> fullLabels = arma::join_rows(trainingLabels, randomLabels);

  // Build the tree.
  DecisionTree<InformationGain> tree(fullData, di, fullLabels, 5, weights, 10);

  // Now evaluate the accuracy of the tree.
  arma::Row<size_t> predictions;
  tree.Classify(testData, predictions);

  BOOST_REQUIRE_EQUAL(predictions.n_elem, testData.n_cols);
  size_t correct = 0;
  for (size_t i = 0; i < testData.n_cols; ++i)
    if (testLabels[i] == predictions[i])
      ++correct;

  // Make sure we got at least 70% accuracy.
  const double correctPct = double(correct) / double(testData.n_cols);
  BOOST_REQUIRE_GT(correctPct, 0.70);
}

BOOST_AUTO_TEST_SUITE_END();
