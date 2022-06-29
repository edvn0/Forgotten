#include "Random.hpp"

namespace Forgotten {

std::mt19937 Random::random_engine;
std::uniform_int_distribution<std::mt19937::result_type> Random::random_distribution;

}