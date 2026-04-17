#include "VlmInferenceEngine.hpp"

// llama.cpp public API.
// F-Prime defines DEPRECATED identically; silence the redefinition warning.
#pragma push_macro("DEPRECATED")
#undef DEPRECATED
#include "llama.h"
#pragma pop_macro("DEPRECATED")

// mtmd: multimodal (vision encoder) extension
#include <time.h>

#include <cstdio>
#include <cstring>

#include "mtmd-helper.h"
#include "mtmd.h"

namespace Orion {

// ---------------------------------------------------------------------------
// Model paths — text decoder and vision encoder are separate GGUF files.
// Override via env vars: ORION_getGgufPath(), ORION_getMmprojPath()
// ---------------------------------------------------------------------------
static const char* getGgufPath() {
    const char* p = ::getenv("ORION_getGgufPath()");
    return p ? p : "/home/pi/ORION/ground_segment/training/orion-q4_k_m.gguf";
}
static const char* getMmprojPath() {
    const char* p = ::getenv("ORION_getMmprojPath()");
    return p ? p : "/home/pi/ORION/ground_segment/training/orion-mmproj-f16.gguf";
}

// ---------------------------------------------------------------------------
// Inference parameters
// ---------------------------------------------------------------------------
static constexpr int IMAGE_W = 512;
static constexpr int IMAGE_H = 512;
static constexpr int N_CTX = 4096;
static constexpr int N_BATCH = 512;
static constexpr int N_THREADS = 4;  // Pi 5 Cortex-A76 quad-core
static constexpr int MAX_RESPONSE_TOKENS = 128;
static constexpr int IMAGE_MAX_TOKENS = 1024;  // cap to save KV space

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

VlmInferenceEngine::VlmInferenceEngine(const char* compName)
    : VlmInferenceEngineComponentBase(compName),
      m_model(nullptr),
      m_ctx(nullptr),
      m_mtmd(nullptr),
      m_sampler(nullptr),
      m_totalInferences(0),
      m_inferenceFailures(0) {}

VlmInferenceEngine::~VlmInferenceEngine() { freeModel(); }

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

void VlmInferenceEngine::LOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_model) {
        // Already loaded — treat as success.
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // Load text model (700 MB .gguf). CPU-only: n_gpu_layers = 0.
    fprintf(stderr, "[VLM] Loading model from: %s\n", getGgufPath());

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;

    m_model = llama_model_load_from_file(getGgufPath(), mparams);
    if (!m_model) {
        fprintf(stderr, "[VLM] llama_model_load_from_file FAILED\n");
        this->log_WARNING_HI_ModelLoadFailed(Fw::String(getGgufPath()));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    fprintf(stderr, "[VLM] Text model loaded OK\n");

    // Create inference context.
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = static_cast<uint32_t>(N_CTX);
    cparams.n_batch = static_cast<uint32_t>(N_BATCH);
    cparams.n_threads = N_THREADS;

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        this->log_WARNING_HI_ModelLoadFailed(Fw::String(getGgufPath()));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Load vision encoder from the same combined GGUF. CPU-only.
    mtmd_context_params vparams = mtmd_context_params_default();
    vparams.use_gpu = false;
    vparams.print_timings = false;
    vparams.n_threads = N_THREADS;
    vparams.image_max_tokens = IMAGE_MAX_TOKENS;

    fprintf(stderr, "[VLM] Loading vision encoder from: %s\n", getMmprojPath());
    m_mtmd = mtmd_init_from_file(getMmprojPath(), m_model, vparams);
    if (!m_mtmd) {
        fprintf(stderr, "[VLM] mtmd_init_from_file FAILED\n");
        llama_free(m_ctx);
        llama_model_free(m_model);
        m_ctx = nullptr;
        m_model = nullptr;
        this->log_WARNING_HI_ModelLoadFailed(Fw::String(getGgufPath()));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Greedy sampler: temperature 0 gives deterministic classification.
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    m_sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(m_sampler, llama_sampler_init_greedy());

    this->log_ACTIVITY_HI_ModelLoaded();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void VlmInferenceEngine::UNLOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    freeModel();
    this->log_ACTIVITY_HI_ModelUnloaded();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ---------------------------------------------------------------------------
// Inference port handler
// ---------------------------------------------------------------------------

void VlmInferenceEngine::pingIn_handler(FwIndexType portNum, U32 key) {
    // Echo key back immediately. If a Ping arrives during a 45-second inference
    // it will queue and be answered as soon as this thread becomes free — the
    // watchdog timeout for this component must be configured above 60 seconds.
    this->pingOut_out(0, key);
}

void VlmInferenceEngine::inferenceRequestIn_handler(FwIndexType portNum, Fw::Buffer& buffer, F64 lat, F64 lon) {
    if (!m_model || !m_ctx || !m_mtmd) {
        // Model not resident — drop the frame and recycle the buffer.
        this->bufferReturnOut_out(0, buffer);
        return;
    }

    struct timespec ts0, ts1;
    ::clock_gettime(CLOCK_MONOTONIC, &ts0);

    Orion::TriagePriority verdict = TriagePriority::LOW;
    char reason[256] = "Low confidence classification";

    bool ok = runInference(buffer, lat, lon, verdict, reason, sizeof(reason));

    ::clock_gettime(CLOCK_MONOTONIC, &ts1);
    U32 elapsed_ms = static_cast<U32>((ts1.tv_sec - ts0.tv_sec) * 1000u + (ts1.tv_nsec - ts0.tv_nsec) / 1000000u);

    if (ok) {
        m_totalInferences++;
        this->tlmWrite_TotalInferences(m_totalInferences);
        this->tlmWrite_InferenceTime_Ms(elapsed_ms);
        this->log_ACTIVITY_LO_InferenceComplete(Fw::String(verdictToStr(verdict)), elapsed_ms);

        // Buffer ownership transfers to TriageRouter.
        Fw::String reasonStr(reason);
        this->triageDecisionOut_out(0, verdict, reasonStr, buffer);
    } else {
        m_inferenceFailures++;
        this->tlmWrite_InferenceFailures(m_inferenceFailures);
        this->log_WARNING_HI_InferenceFailed();
        this->bufferReturnOut_out(0, buffer);
    }
}

// ---------------------------------------------------------------------------
// runInference — core llama.cpp / mtmd pipeline
// ---------------------------------------------------------------------------

bool VlmInferenceEngine::runInference(const Fw::Buffer& buffer, F64 lat, F64 lon, Orion::TriagePriority& verdict,
                                      char* reason, FwSizeType reasonLen) {
    // Build the chat-formatted prompt.
    // mtmd_default_marker() returns the model-specific image placeholder token
    // (e.g. "<|image_1|>" for Phi-3 Vision). mtmd_tokenize() replaces it with
    // the actual vision encoder output tokens.
    char prompt[768];
    ::snprintf(prompt, sizeof(prompt),
               "<|user|>\n%s\n"
               "GPS: Lat=%.6f, Lon=%.6f\n"
               "Classify this satellite image as HIGH, MEDIUM, or LOW priority.\n"
               "  HIGH   : military assets, infrastructure damage, emergency events\n"
               "  MEDIUM : vessels, agriculture, construction, urban activity\n"
               "  LOW    : open ocean, desert, cloud cover, featureless terrain\n"
               "Respond ONLY with JSON: "
               "{\"verdict\": \"HIGH\"|\"MEDIUM\"|\"LOW\", \"reason\": \"<25 words\"}\n"
               "<|end|>\n<|assistant|>",
               mtmd_default_marker(), lat, lon);

    // Wrap the raw 512×512 RGB pixel data from the buffer.
    mtmd_bitmap* bmp =
        mtmd_bitmap_init(static_cast<uint32_t>(IMAGE_W), static_cast<uint32_t>(IMAGE_H), buffer.getData());
    if (!bmp) {
        return false;
    }

    // Tokenize: the image marker in the prompt is replaced with vision tokens.
    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    mtmd_input_text text_in = {prompt, true, true};
    const mtmd_bitmap* bitmaps[] = {bmp};

    int32_t ret = mtmd_tokenize(m_mtmd, chunks, &text_in, bitmaps, 1);
    mtmd_bitmap_free(bmp);

    if (ret != 0) {
        mtmd_input_chunks_free(chunks);
        return false;
    }

    // Evaluate all chunks (text + encoded image) into the KV cache.
    // new_n_past is updated to the position after the last prompt token.
    llama_pos new_n_past = 0;
    ret = mtmd_helper_eval_chunks(m_mtmd, m_ctx, chunks, 0, 0, N_BATCH,
                                  /*logits_last=*/true, &new_n_past);
    mtmd_input_chunks_free(chunks);

    if (ret != 0) {
        llama_memory_clear(llama_get_memory(m_ctx), false);
        return false;
    }

    // Generate response tokens one at a time.
    // llama_batch_get_one with nullptr pos auto-advances from the KV tail.
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    char response[512] = {};
    int rpos = 0;

    for (int i = 0; i < MAX_RESPONSE_TOKENS && rpos < 510; i++) {
        llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
        llama_sampler_accept(m_sampler, tok);

        if (llama_vocab_is_eog(vocab, tok)) {
            break;
        }

        char piece[32] = {};
        int n = llama_token_to_piece(vocab, tok, piece, static_cast<int32_t>(sizeof(piece)) - 1, 0, true);
        if (n > 0 && rpos + n < 511) {
            ::memcpy(response + rpos, piece, static_cast<size_t>(n));
            rpos += n;
        }

        if (llama_decode(m_ctx, llama_batch_get_one(&tok, 1)) != 0) {
            break;
        }
    }

    // Reset KV cache and sampler so the next frame starts clean.
    llama_memory_clear(llama_get_memory(m_ctx), false);
    llama_sampler_reset(m_sampler);

    parseVerdictJson(response, verdict, reason, reasonLen);
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void VlmInferenceEngine::parseVerdictJson(const char* json, Orion::TriagePriority& verdict, char* reason,
                                          FwSizeType reasonLen) {
    verdict = TriagePriority::LOW;

    const char* vp = ::strstr(json, "\"verdict\"");
    if (vp) {
        if (::strstr(vp, "\"HIGH\""))
            verdict = TriagePriority::HIGH;
        else if (::strstr(vp, "\"MEDIUM\""))
            verdict = TriagePriority::MEDIUM;
    }

    const char* rp = ::strstr(json, "\"reason\"");
    if (rp) {
        const char* start = ::strchr(rp, ':');
        if (start) {
            start = ::strchr(start, '"');
            if (start) {
                ++start;
                const char* end = ::strchr(start, '"');
                if (end) {
                    FwSizeType len = static_cast<FwSizeType>(end - start);
                    if (len >= reasonLen) len = reasonLen - 1;
                    ::memcpy(reason, start, len);
                    reason[len] = '\0';
                    return;
                }
            }
        }
    }

    // Malformed JSON — provide a safe fallback.
    ::strncpy(reason, "Unstructured model response", static_cast<size_t>(reasonLen) - 1);
    reason[reasonLen - 1] = '\0';
}

const char* VlmInferenceEngine::verdictToStr(const Orion::TriagePriority& v) {
    switch (v.e) {
        case TriagePriority::HIGH:
            return "HIGH";
        case TriagePriority::MEDIUM:
            return "MEDIUM";
        case TriagePriority::LOW:
            return "LOW";
        default:
            return "UNKNOWN";
    }
}

void VlmInferenceEngine::freeModel() {
    if (m_sampler) {
        llama_sampler_free(m_sampler);
        m_sampler = nullptr;
    }
    if (m_mtmd) {
        mtmd_free(m_mtmd);
        m_mtmd = nullptr;
    }
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
}

}  // namespace Orion
