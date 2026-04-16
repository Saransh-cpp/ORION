#ifndef ORION_VLM_INFERENCE_ENGINE_HPP
#define ORION_VLM_INFERENCE_ENGINE_HPP

#include "Orion/Components/VlmInferenceEngine/VlmInferenceEngineComponentAc.hpp"

// Forward-declare llama.cpp / mtmd opaque types to keep them out of
// F-Prime-generated headers that are compiled without llama.cpp includes.
struct llama_model;
struct llama_context;
struct mtmd_context;
struct llama_sampler;

namespace Orion {

class VlmInferenceEngine final : public VlmInferenceEngineComponentBase {
  public:
    explicit VlmInferenceEngine(const char* compName);
    ~VlmInferenceEngine();

  private:
    // -----------------------------------------------------------------------
    // Command handlers (run on the component thread)
    // -----------------------------------------------------------------------

    void LOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void UNLOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // -----------------------------------------------------------------------
    // Port handler
    // -----------------------------------------------------------------------

    void inferenceRequestIn_handler(FwIndexType portNum, Fw::Buffer& buffer, F64 lat, F64 lon) override;

    // Health Watchdog: echo the key back immediately when dequeued.
    void pingIn_handler(FwIndexType portNum, U32 key) override;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    //! Runs the full llama.cpp forward pass for one frame.
    //! On success, fills verdict and reason and returns true.
    //! On failure, returns false (caller must return buffer to pool).
    bool runInference(const Fw::Buffer& buffer, F64 lat, F64 lon, Orion::TriagePriority& verdict, char* reason,
                      FwSizeType reasonLen);

    //! Parses {"verdict": "HIGH"|"MEDIUM"|"LOW", "reason": "..."} from raw text.
    static void parseVerdictJson(const char* json, Orion::TriagePriority& verdict, char* reason, FwSizeType reasonLen);

    static const char* verdictToStr(const Orion::TriagePriority& v);

    //! Frees all llama.cpp state. Safe to call when already unloaded.
    void freeModel();

    // -----------------------------------------------------------------------
    // State — raw pointers because llama.cpp is a C API
    // -----------------------------------------------------------------------

    llama_model* m_model;
    llama_context* m_ctx;
    mtmd_context* m_mtmd;
    llama_sampler* m_sampler;

    U32 m_totalInferences;
    U32 m_inferenceFailures;
};

}  // namespace Orion

#endif  // ORION_VLM_INFERENCE_ENGINE_HPP
