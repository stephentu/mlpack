/**
 * @file rl_environment_test.hpp
 * @author Shangtong Zhang
 *
 * Basic test for the components of reinforcement learning algorithms.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */

#include <mlpack/core.hpp>

#include <mlpack/methods/reinforcement_learning/environment/mountain_car.hpp>
#include <mlpack/methods/reinforcement_learning/environment/cart_pole.hpp>
#include <mlpack/methods/reinforcement_learning/replay/random_replay.hpp>

#include <boost/test/unit_test.hpp>
#include "test_tools.hpp"

using namespace mlpack;
using namespace mlpack::rl;

BOOST_AUTO_TEST_SUITE(RLComponentsTest)

/**
 * Constructs a MountainCar instance and check if the main rountine works as
 * it should be.
 */
BOOST_AUTO_TEST_CASE(SimpleMountainCarTest)
{
  const MountainCar task = MountainCar();

  MountainCar::State state = task.InitialSample();
  MountainCar::Action action = MountainCar::Action::backward;
  double reward = task.Sample(state, action);

  BOOST_REQUIRE_EQUAL(reward, -1.0);
  BOOST_REQUIRE(!task.IsTerminal(state));
  BOOST_REQUIRE_EQUAL(3, MountainCar::Action::size);
}

/**
 * Constructs a CartPole instance and check if the main rountine works as
 * it should be.
 */
BOOST_AUTO_TEST_CASE(SimpleCartPoleTest)
{
  const CartPole task = CartPole();

  CartPole::State state = task.InitialSample();
  CartPole::Action action = CartPole::Action::backward;
  double reward = task.Sample(state, action);

  BOOST_REQUIRE_EQUAL(reward, 1.0);
  BOOST_REQUIRE(!task.IsTerminal(state));
  BOOST_REQUIRE_EQUAL(2, CartPole::Action::size);
}

/**
 * Construct a random replay instance and check if it works as
 * it should be.
 */
BOOST_AUTO_TEST_CASE(RandomReplayTest)
{
  RandomReplay<MountainCar> replay(1, 3);
  MountainCar env;
  MountainCar::State state = env.InitialSample();
  MountainCar::Action action = MountainCar::Action::forward;
  MountainCar::State nextState;
  double reward = env.Sample(state, action, nextState);
  replay.Store(state, action, reward, nextState, env.IsTerminal(nextState));
  arma::mat sampledState;
  arma::icolvec sampledAction;
  arma::colvec sampledReward;
  arma::mat sampledNextState;
  arma::icolvec sampledTerminal;

  //! So far there should be only one record in the memory
  replay.Sample(sampledState, sampledAction, sampledReward, sampledNextState, sampledTerminal);
  CheckMatrices(state.Encode(), sampledState);
  BOOST_REQUIRE_EQUAL(action, arma::as_scalar(sampledAction));
  BOOST_REQUIRE_CLOSE(reward, arma::as_scalar(sampledReward), 1e-5);
  CheckMatrices(nextState.Encode(), sampledNextState);
  BOOST_REQUIRE_EQUAL(false, arma::as_scalar(sampledTerminal));
  BOOST_REQUIRE_EQUAL(1, replay.Size());

  //! Overwrite the memory with a nonsense record
  for (size_t i = 0; i < 5; ++i) {
    replay.Store(nextState, action, reward, state, true);
  }
  BOOST_REQUIRE_EQUAL(3, replay.Size());

  //! Sample several times, the original record shouldn't appear
  for (size_t i = 0; i < 30; ++i) {
    replay.Sample(sampledState, sampledAction, sampledReward, sampledNextState, sampledTerminal);
    CheckMatrices(state.Encode(), sampledNextState);
    CheckMatrices(nextState.Encode(), sampledState);
    BOOST_REQUIRE_EQUAL(true, arma::as_scalar(sampledTerminal));
  }
}

BOOST_AUTO_TEST_SUITE_END()
