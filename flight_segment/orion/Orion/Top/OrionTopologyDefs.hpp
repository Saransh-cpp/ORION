#ifndef ORION_ORIONTOPOLOGYDEFS_HPP
#define ORION_ORIONTOPOLOGYDEFS_HPP

// Subtopology PingEntries
#include "Svc/Subtopologies/CdhCore/PingEntries.hpp"
#include "Svc/Subtopologies/ComCcsds/PingEntries.hpp"
#include "Svc/Subtopologies/DataProducts/PingEntries.hpp"
#include "Svc/Subtopologies/FileHandling/PingEntries.hpp"

// Subtopology TopologyDefs
#include "Svc/Subtopologies/CdhCore/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/ComCcsds/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/DataProducts/SubtopologyTopologyDefs.hpp"
#include "Svc/Subtopologies/FileHandling/SubtopologyTopologyDefs.hpp"

// ComCcsds port-index enums
#include "Svc/Subtopologies/ComCcsds/Ports_ComBufferQueueEnumAc.hpp"
#include "Svc/Subtopologies/ComCcsds/Ports_ComPacketQueueEnumAc.hpp"

// Autocoded FPP constants
#include "Orion/Top/FppConstantsAc.hpp"

// ---------------------------------------------------------------------------
// Health watchdog ping thresholds
//
// Each namespace names a component instance.  WARN/FATAL specify how many
// consecutive missed pings trigger WARNING_HI / FATAL events respectively.
// At the default health check rate (rate group 3 at 0.25 Hz = every 4 s),
// WARN=3 / FATAL=5 means ~12 s / ~20 s.
// ---------------------------------------------------------------------------

namespace PingEntries {
namespace Orion_rateGroup1Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace Orion_rateGroup2Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace Orion_rateGroup3Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace Orion_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}

}  // namespace PingEntries

// ---------------------------------------------------------------------------

namespace Orion {

struct TopologyState {
    const char* hostname;
    U16 port;
    CdhCore::SubtopologyState cdhCore;
    ComCcsds::SubtopologyState comCcsds;
    DataProducts::SubtopologyState dataProducts;
    FileHandling::SubtopologyState fileHandling;
};

namespace PingEntries = ::PingEntries;
}  // namespace Orion

#endif  // ORION_ORIONTOPOLOGYDEFS_HPP
