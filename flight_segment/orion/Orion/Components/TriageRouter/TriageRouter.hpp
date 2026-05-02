#ifndef ORION_TRIAGE_ROUTER_HPP
#define ORION_TRIAGE_ROUTER_HPP

#include "Orion/Components/TriageRouter/TriageRouterComponentAc.hpp"

namespace Orion {

class TriageRouter final : public TriageRouterComponentBase {
  public:
    explicit TriageRouter(const char* compName);
    ~TriageRouter();

  private:
    // -----------------------------------------------------------------------
    // Port handler
    // -----------------------------------------------------------------------

    void triageDecisionIn_handler(FwIndexType portNum, const Orion::TriagePriority& verdict,
                                  const Fw::StringBase& reason, Fw::Buffer& buffer) override;

    //! Mode change handler: stores the current mission mode.
    void modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) override;

    // -----------------------------------------------------------------------
    // Routing helpers
    // -----------------------------------------------------------------------

    //! Forwards buffer to GroundCommsDriver for immediate X-Band downlink.
    void routeHigh(const Fw::StringBase& reason, Fw::Buffer& buffer);

    //! Writes buffer to microSD bulk storage, then returns buffer to pool.
    void routeMedium(Fw::Buffer& buffer);

    //! Returns buffer to pool immediately: no data is retained.
    void routeLow(Fw::Buffer& buffer);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    U32 m_highRouted;
    U32 m_mediumSaved;
    U32 m_lowDiscarded;
    U32 m_mediumFileIndex;      //!< Monotonic counter for unique MEDIUM filenames.
    MissionMode m_currentMode;  //!< Current mission mode from EventAction
};

}  // namespace Orion

#endif  // ORION_TRIAGE_ROUTER_HPP
