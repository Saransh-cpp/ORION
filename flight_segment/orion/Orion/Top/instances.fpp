module Orion {

  # ----------------------------------------------------------------------
  # Base ID Convention: 0xDSSCCxxx
  #   D=1 (Orion deployment), SS=00 (main topology), CC=component
  # Subtopologies use their own base ID ranges internally.
  # ----------------------------------------------------------------------

  module Default {
    constant QUEUE_SIZE = 10
    constant STACK_SIZE = 64 * 1024
  }

  # ----------------------------------------------------------------------
  # Active component instances — standard F-Prime infrastructure
  # ----------------------------------------------------------------------

  instance rateGroup1Comp: Svc.ActiveRateGroup base id 0x10001000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 43

  instance rateGroup2Comp: Svc.ActiveRateGroup base id 0x10002000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 42

  instance rateGroup3Comp: Svc.ActiveRateGroup base id 0x10003000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 41

  instance cmdSeq: Svc.CmdSequencer base id 0x10004000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 40

  # ----------------------------------------------------------------------
  # Active component instances — ORION mission pipeline
  # ----------------------------------------------------------------------

  instance cameraManager: Orion.CameraManager base id 0x10005000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 30

  # Lowest priority of all active components. The 15-45 s llama.cpp
  # forward pass must never preempt telemetry, routing, or comms.
  # Stack is 256 KB for llama.cpp's stack-heavy inference path.
  # Queue is 5: at most ~13 images per LEO pass, and inference is slow
  # enough that a larger queue would just waste memory.
  instance vlmInferenceEngine: Orion.VlmInferenceEngine base id 0x10006000 \
    queue size 5 \
    stack size 256 * 1024 \
    priority 10

  instance triageRouter: Orion.TriageRouter base id 0x10007000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 25

  instance groundCommsDriver: Orion.GroundCommsDriver base id 0x10008000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 20

  # ----------------------------------------------------------------------
  # Passive component instances — standard F-Prime infrastructure
  # ----------------------------------------------------------------------

  instance posixTime: Svc.PosixTime base id 0x10020000

  instance rateGroupDriverComp: Svc.RateGroupDriver base id 0x10021000

  instance systemResources: Svc.SystemResources base id 0x10023000

  instance linuxTimer: Svc.LinuxTimer base id 0x10024000

  instance comDriver: Drv.TcpClient base id 0x10025000

  # ----------------------------------------------------------------------
  # Active component instances — ORION mission (NavTelemetry)
  # ----------------------------------------------------------------------

  # NavTelemetry polls SimSat for orbital position every 5 seconds
  # and computes comm window state. Priority above VLM but below comms.
  instance navTelemetry: Orion.NavTelemetry base id 0x10015000 \
    queue size Default.QUEUE_SIZE \
    stack size Default.STACK_SIZE \
    priority 35

  # ----------------------------------------------------------------------
  # Passive component instances — ORION mission
  # ----------------------------------------------------------------------

  # Static pool of 20 × 786432-byte (512×512×3 RGB) image buffers.
  # ~15.7 MB total; remaining Pi 5 RAM is dedicated to the VLM weights.
  instance bufferManager: Svc.BufferManager base id 0x10016000 \
  {
    phase Fpp.ToCpp.Phases.configObjects """
    Svc::BufferManager::BufferBins bins;
    Fw::MallocAllocator allocator;
    """

    phase Fpp.ToCpp.Phases.configComponents """
    memset(&ConfigObjects::Orion_bufferManager::bins, 0, sizeof(ConfigObjects::Orion_bufferManager::bins));
    ConfigObjects::Orion_bufferManager::bins.bins[0].bufferSize = 786432;
    ConfigObjects::Orion_bufferManager::bins.bins[0].numBuffers = 20;
    Orion::bufferManager.setup(
        100,
        0,
        ConfigObjects::Orion_bufferManager::allocator,
        ConfigObjects::Orion_bufferManager::bins
    );
    """

    phase Fpp.ToCpp.Phases.tearDownComponents """
    Orion::bufferManager.cleanup();
    """
  }

}
