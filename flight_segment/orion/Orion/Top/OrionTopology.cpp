// Provides access to autocoded topology functions
#include <Orion/Top/OrionTopologyAc.hpp>

// Allocator for command sequencer buffer
#include <Fw/Types/MallocAllocator.hpp>

// Allow unqualified access to Orion namespace members
using namespace Orion;

// Allocator used by the command sequencer for its internal buffer
Fw::MallocAllocator mallocator;

// The rate group driver divides the 1 Hz timer tick into three groups:
//   Rate Group 1: 1 Hz   (divider 1, offset 0)
//   Rate Group 2: 0.5 Hz (divider 2, offset 0)
//   Rate Group 3: 0.25 Hz (divider 4, offset 0)
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{{1, 0}, {2, 0}, {4, 0}}};

// Context tokens for rate group members (unused are set to zero)
U32 rateGroup1Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
U32 rateGroup2Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};
U32 rateGroup3Context[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};

enum TopologyConstants {
    COMM_PRIORITY = 34,
};

/**
 * Configure components that require project-specific setup beyond what the
 * autocoder handles.
 */
void configureTopology() {
    rateGroupDriverComp.configure(rateGroupDivisorsSet);

    rateGroup1Comp.configure(rateGroup1Context, FW_NUM_ARRAY_ELEMENTS(rateGroup1Context));
    rateGroup2Comp.configure(rateGroup2Context, FW_NUM_ARRAY_ELEMENTS(rateGroup2Context));
    rateGroup3Comp.configure(rateGroup3Context, FW_NUM_ARRAY_ELEMENTS(rateGroup3Context));

    // Command sequencer needs a buffer for holding command sequences
    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);
}

namespace Orion {

void setupTopology(const TopologyState& state) {
    // Autocoded initialization
    initComponents(state);
    setBaseIds();
    connectComponents();
    regCommands();
    configComponents(state);

    // Configure TCP comm driver (connects to fprime-gds)
    if (state.hostname != nullptr && state.port != 0) {
        comDriver.configure(state.hostname, state.port);
    }

    // Project-specific configuration
    configureTopology();

    // Load parameters from persistent storage
    loadParameters();

    // Start active component tasks
    startTasks(state);

    // Start the comm driver task (must come after active components)
    if (state.hostname != nullptr && state.port != 0) {
        Os::TaskString name("ReceiveTask");
        comDriver.start(name, COMM_PRIORITY, Default::STACK_SIZE);
    }
}

void startRateGroups(const Fw::TimeInterval& interval) {
    // Blocks until stopRateGroups() is called (from signal handler).
    linuxTimer.startTimer(interval);
}

void stopRateGroups() { linuxTimer.quit(); }

void teardownTopology(const TopologyState& state) {
    // Stop active component tasks
    stopTasks(state);
    freeThreads(state);

    // Stop the comm driver
    comDriver.stop();
    (void)comDriver.join();

    // Deallocate resources
    cmdSeq.deallocateBuffer(mallocator);
    tearDownComponents(state);
}

}  // namespace Orion
