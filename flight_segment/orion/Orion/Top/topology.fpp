module Orion {

  # ----------------------------------------------------------------------
  # Symbolic constants for rate group port indices
  # ----------------------------------------------------------------------

  enum Ports_RateGroups {
    rateGroup1
    rateGroup2
    rateGroup3
  }

  topology Orion {

    # ------------------------------------------------------------------
    # Subtopology instances — standard F-Prime infrastructure
    # ------------------------------------------------------------------

    instance CdhCore.Subtopology
    instance ComCcsds.Subtopology
    instance FileHandling.Subtopology
    instance DataProducts.Subtopology

    # ------------------------------------------------------------------
    # Instances used in this topology
    # ------------------------------------------------------------------

    instance posixTime
    instance rateGroup1Comp
    instance rateGroup2Comp
    instance rateGroup3Comp
    instance rateGroupDriverComp
    instance systemResources
    instance linuxTimer
    instance comDriver
    instance cmdSeq

    # ORION mission instances
    instance eventAction
    instance navTelemetry
    instance cameraManager
    instance vlmInferenceEngine
    instance triageRouter
    instance groundCommsDriver
    instance bufferManager

    # ------------------------------------------------------------------
    # Pattern graph specifiers — auto-wired framework services
    # ------------------------------------------------------------------

    command connections instance CdhCore.cmdDisp

    event connections instance CdhCore.events

    telemetry connections instance CdhCore.tlmSend

    text event connections instance CdhCore.textLogger

    health connections instance CdhCore.$health

    param connections instance FileHandling.prmDb

    time connections instance posixTime

    # ------------------------------------------------------------------
    # Rate group connections
    # ------------------------------------------------------------------

    connections RateGroups {

      # Linux timer drives the rate group driver at 1 Hz
      linuxTimer.CycleOut -> rateGroupDriverComp.CycleIn

      # Rate group 1 (1 Hz) — telemetry, file downlink, comms, SimSat polling, auto-capture
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1Comp.CycleIn
      rateGroup1Comp.RateGroupMemberOut[0] -> CdhCore.Subtopology.tlmSendRun
      rateGroup1Comp.RateGroupMemberOut[1] -> FileHandling.Subtopology.fileDownlinkRun
      rateGroup1Comp.RateGroupMemberOut[2] -> systemResources.run
      rateGroup1Comp.RateGroupMemberOut[3] -> ComCcsds.Subtopology.comQueueRun
      rateGroup1Comp.RateGroupMemberOut[4] -> CdhCore.Subtopology.cmdDispRun
      rateGroup1Comp.RateGroupMemberOut[5] -> ComCcsds.Subtopology.aggregatorTimeout
      rateGroup1Comp.RateGroupMemberOut[6] -> navTelemetry.schedIn
      rateGroup1Comp.RateGroupMemberOut[7] -> cameraManager.schedIn
      rateGroup1Comp.RateGroupMemberOut[8] -> groundCommsDriver.schedIn
      rateGroup1Comp.RateGroupMemberOut[9] -> eventAction.schedIn

      # Rate group 2 (0.5 Hz) — sequencer
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2Comp.CycleIn
      rateGroup2Comp.RateGroupMemberOut[0] -> cmdSeq.schedIn

      # Rate group 3 (0.25 Hz) — health, buffer managers, data products
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup3] -> rateGroup3Comp.CycleIn
      rateGroup3Comp.RateGroupMemberOut[0] -> CdhCore.Subtopology.healthRun
      rateGroup3Comp.RateGroupMemberOut[1] -> ComCcsds.Subtopology.bufferManagerSchedIn
      rateGroup3Comp.RateGroupMemberOut[2] -> DataProducts.Subtopology.dpBufferManagerSchedIn
      rateGroup3Comp.RateGroupMemberOut[3] -> DataProducts.Subtopology.dpWriterSchedIn
      rateGroup3Comp.RateGroupMemberOut[4] -> DataProducts.Subtopology.dpMgrSchedIn
      rateGroup3Comp.RateGroupMemberOut[5] -> bufferManager.schedIn
    }

    # ------------------------------------------------------------------
    # Communication driver connections
    # ------------------------------------------------------------------

    connections Communications {
      comDriver.allocate   -> ComCcsds.Subtopology.commsBufferGetCallee
      comDriver.deallocate -> ComCcsds.Subtopology.commsBufferSendIn

      comDriver.$recv                          -> ComCcsds.Subtopology.drvReceiveIn
      ComCcsds.Subtopology.drvReceiveReturnOut -> comDriver.recvReturnIn

      ComCcsds.Subtopology.drvSendOut -> comDriver.$send
      comDriver.ready                 -> ComCcsds.Subtopology.drvConnected
    }

    # ------------------------------------------------------------------
    # Subtopology cross-connections
    # ------------------------------------------------------------------

    connections ComCcsds_CdhCore {
      CdhCore.Subtopology.eventsPktSend  -> ComCcsds.Subtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.EVENTS]
      CdhCore.Subtopology.tlmSendPktSend -> ComCcsds.Subtopology.comPacketQueueIn[ComCcsds.Ports_ComPacketQueue.TELEMETRY]

      ComCcsds.Subtopology.commandOut        -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> ComCcsds.Subtopology.cmdResponseIn
      cmdSeq.comCmdOut                       -> CdhCore.Subtopology.seqCmdBuff
      CdhCore.Subtopology.seqCmdStatus       -> cmdSeq.cmdResponseIn
    }

    connections ComCcsds_FileHandling {
      FileHandling.Subtopology.fileDownlinkBufferSendOut -> ComCcsds.Subtopology.bufferQueueIn[ComCcsds.Ports_ComBufferQueue.FILE]
      ComCcsds.Subtopology.bufferReturnOut[ComCcsds.Ports_ComBufferQueue.FILE] -> FileHandling.Subtopology.fileDownlinkBufferReturn

      ComCcsds.Subtopology.fileUplinkOut                -> FileHandling.Subtopology.fileUplinkBufferSendIn
      FileHandling.Subtopology.fileUplinkBufferSendOut -> ComCcsds.Subtopology.fileUplinkReturnIn
    }

    connections FileHandling_DataProducts {
      DataProducts.Subtopology.dpCatFileOut              -> FileHandling.Subtopology.fileDownlinkSendFile
      FileHandling.Subtopology.fileDownlinkFileComplete  -> DataProducts.Subtopology.dpCatFileDone
    }

    # ------------------------------------------------------------------
    # ORION mission data flow
    # ------------------------------------------------------------------

    connections OrionPipeline {
      # CameraManager checks out 786 KB buffers from the image pool
      cameraManager.bufferGetOut -> bufferManager.bufferGetCallee

      # CameraManager synchronously reads GPS at capture time
      cameraManager.navStateOut -> navTelemetry.navStateGet

      # CameraManager dispatches image + GPS to VLM (async, fire-and-forget)
      cameraManager.inferenceRequestOut -> vlmInferenceEngine.inferenceRequestIn

      # CameraManager returns failed/empty buffers to pool
      cameraManager.bufferReturnOut -> bufferManager.bufferSendIn

      # VLM forwards classified result + buffer to triage router
      vlmInferenceEngine.triageDecisionOut -> triageRouter.triageDecisionIn

      # VLM returns buffer on inference failure
      vlmInferenceEngine.bufferReturnOut -> bufferManager.bufferSendIn

      # TriageRouter routes HIGH images to GroundCommsDriver for immediate downlink
      triageRouter.fileDownlinkOut -> groundCommsDriver.fileDownlinkIn

      # TriageRouter returns MEDIUM/LOW buffers to pool
      triageRouter.bufferReturnOut -> bufferManager.bufferSendIn

      # GroundCommsDriver returns buffer after TCP transmit
      groundCommsDriver.bufferReturnOut -> bufferManager.bufferSendIn

      # EventAction queries NavTelemetry for position and comm window state
      eventAction.navStateIn -> navTelemetry.navStateGet

      # EventAction broadcasts mode changes to all pipeline components
      eventAction.modeChangeOut[0] -> cameraManager.modeChangeIn
      eventAction.modeChangeOut[1] -> groundCommsDriver.modeChangeIn
      eventAction.modeChangeOut[2] -> vlmInferenceEngine.modeChangeIn
      eventAction.modeChangeOut[3] -> triageRouter.modeChangeIn

      # EventAction queues MEDIUM file downloads via F-Prime FileDownlink
      eventAction.sendFileOut -> FileHandling.Subtopology.fileDownlinkSendFile
    }

  }

}
