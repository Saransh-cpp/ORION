#ifndef ORION_ORIONTOPOLOGY_HPP
#define ORION_ORIONTOPOLOGY_HPP

#include <Orion/Top/OrionTopologyDefs.hpp>

namespace Orion {

void setupTopology(const TopologyState& state);
void teardownTopology(const TopologyState& state);

void startRateGroups(const Fw::TimeInterval& interval);
void stopRateGroups();

}  // namespace Orion

#endif  // ORION_ORIONTOPOLOGY_HPP
