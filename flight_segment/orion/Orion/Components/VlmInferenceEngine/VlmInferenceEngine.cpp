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

static const char* modeStr(MissionMode mode) {
    switch (mode.e) {
        case MissionMode::IDLE:
            return "IDLE";
        case MissionMode::MEASURE:
            return "MEASURE";
        case MissionMode::DOWNLINK:
            return "DOWNLINK";
        case MissionMode::SAFE:
            return "SAFE";
        default:
            return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Model paths — text decoder and vision encoder are separate GGUF files.
// Override via env vars: ORION_GGUF_PATH, ORION_MMPROJ_PATH
// ---------------------------------------------------------------------------
static const char* getGgufPath() {
    const char* p = ::getenv("ORION_GGUF_PATH");
    return p ? p : "/home/saransh/ORION/orion-q4_k_m.gguf";
}
static const char* getMmprojPath() {
    const char* p = ::getenv("ORION_MMPROJ_PATH");
    return p ? p : "/home/saransh/ORION/orion-mmproj-f16.gguf";
}

// ---------------------------------------------------------------------------
// Inference parameters
// ---------------------------------------------------------------------------
static constexpr int IMAGE_W = 512;
static constexpr int IMAGE_H = 512;
static constexpr int N_CTX = 4096;
static constexpr int N_BATCH = 512;
static constexpr int N_THREADS = 4;  // Pi 5 Cortex-A76 quad-core
static constexpr int MAX_RESPONSE_TOKENS = 200;
static constexpr int IMAGE_MAX_TOKENS = 1024;    // cap to save KV space
static constexpr int INFERENCE_TIMEOUT_S = 120;  // abort inference after 2 minutes

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
      m_inferenceFailures(0),
      m_currentMode(MissionMode::IDLE) {}

VlmInferenceEngine::~VlmInferenceEngine() { freeModel(); }

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

void VlmInferenceEngine::LOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_currentMode.e != MissionMode::MEASURE && m_currentMode.e != MissionMode::DOWNLINK) {
        this->log_WARNING_LO_LoadModelRejectedWrongMode(Fw::String(modeStr(m_currentMode)));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    bool ok = loadModel();
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void VlmInferenceEngine::UNLOAD_MODEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    freeModel();
    this->log_ACTIVITY_HI_ModelUnloaded();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ---------------------------------------------------------------------------
// Inference port handler
// ---------------------------------------------------------------------------

void VlmInferenceEngine::modeChangeIn_handler(FwIndexType portNum, const Orion::MissionMode& mode) {
    m_currentMode = mode;

    if (mode.e == MissionMode::MEASURE && !m_model) {
        // Entering MEASURE — auto-load the model
        loadModel();
    } else if (mode.e == MissionMode::IDLE || mode.e == MissionMode::SAFE) {
        // In IDLE or SAFE — unload to free RAM
        if (m_model) {
            freeModel();
            this->log_ACTIVITY_HI_ModelUnloaded();
        }
    }
    // DOWNLINK: model stays loaded (short pass, reload is expensive)
}

void VlmInferenceEngine::inferenceRequestIn_handler(FwIndexType portNum, Fw::Buffer& buffer, F64 lat, F64 lon) {
    // In SAFE mode, drop all frames immediately
    if (m_currentMode.e == MissionMode::SAFE) {
        this->bufferReturnOut_out(0, buffer);
        return;
    }

    if (!m_model || !m_ctx || !m_mtmd) {
        // Model not resident — drop the frame and recycle the buffer.
        this->log_WARNING_LO_FrameDroppedModelNotLoaded();
        this->bufferReturnOut_out(0, buffer);
        return;
    }

    struct timespec ts0, ts1;
    ::clock_gettime(CLOCK_MONOTONIC, &ts0);

    Orion::TriagePriority verdict = TriagePriority::LOW;
    char reason[512] = "Low confidence classification";

    bool ok = runInference(buffer, lat, lon, verdict, reason, sizeof(reason));

    ::clock_gettime(CLOCK_MONOTONIC, &ts1);
    U32 elapsed_ms = static_cast<U32>((ts1.tv_sec - ts0.tv_sec) * 1000u + (ts1.tv_nsec - ts0.tv_nsec) / 1000000u);

    if (ok) {
        m_totalInferences++;
        this->tlmWrite_TotalInferences(m_totalInferences);
        this->tlmWrite_InferenceTime_Ms(elapsed_ms);
        this->log_ACTIVITY_HI_InferenceComplete(Fw::String(verdictToStr(verdict)), Fw::String(reason), elapsed_ms);

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
    // Build prompt matching the fine-tuning chat template (ChatML format).
    // HuggingFace apply_chat_template() for LFM2.5-VL produces:
    //   <|startoftext|><|im_start|>user\n<image>\n{prompt}<|im_end|>\n<|im_start|>assistant\n
    // mtmd_default_marker() returns the model's image placeholder token.
    const char* imgMarker = mtmd_default_marker();

    char prompt[1536];
    ::snprintf(prompt, sizeof(prompt),
               "<|im_start|>user\n%s\n"
               "You are an autonomous orbital triage assistant. "
               "Analyze this high-resolution RGB satellite image "
               "captured at Longitude: %.6f, Latitude: %.6f.\n"
               "Strictly use one of these categories based on visual morphology:\n"
               "- HIGH: Extreme-scale strategic anomalies, dense geometric cargo/vessel "
               "infrastructure, massive cooling towers, sprawling runways, or distinct "
               "geological/artificial chokepoints.\n"
               "- MEDIUM: Standard human civilization. Ordinary urban grids, low-density "
               "suburban sprawl, regular checkerboard agriculture, or localized "
               "infrastructure (malls, regional strips).\n"
               "- LOW: Complete absence of human infrastructure. Featureless deep oceans, "
               "unbroken canopy, barren deserts, or purely natural geological formations "
               "(craters, natural cliffs).\n"
               "You MUST output your response as a valid JSON object. To ensure accurate "
               "visual reasoning, you must output the \"reason\" key FIRST, followed by "
               "the \"category\" key.<|im_end|>\n<|im_start|>assistant\n",
               imgMarker, lon, lat);

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

    // Record start time for timeout check.
    struct timespec t0;
    ::clock_gettime(CLOCK_MONOTONIC, &t0);

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

    // Check timeout after prompt eval (can take 30-40s on Pi).
    struct timespec tnow;
    ::clock_gettime(CLOCK_MONOTONIC, &tnow);
    U32 elapsed = static_cast<U32>(tnow.tv_sec - t0.tv_sec);
    if (elapsed >= INFERENCE_TIMEOUT_S) {
        llama_memory_clear(llama_get_memory(m_ctx), false);
        this->log_WARNING_HI_InferenceTimeout(elapsed * 1000u);
        return false;
    }

    // Generate response tokens one at a time.
    // llama_batch_get_one with nullptr pos auto-advances from the KV tail.
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    char response[1024] = {};
    int rpos = 0;

    for (int i = 0; i < MAX_RESPONSE_TOKENS && rpos < 1022; i++) {
        // Per-token timeout check.
        ::clock_gettime(CLOCK_MONOTONIC, &tnow);
        elapsed = static_cast<U32>(tnow.tv_sec - t0.tv_sec);
        if (elapsed >= INFERENCE_TIMEOUT_S) {
            llama_memory_clear(llama_get_memory(m_ctx), false);
            llama_sampler_reset(m_sampler);
            this->log_WARNING_HI_InferenceTimeout(elapsed * 1000u);
            return false;
        }

        llama_token tok = llama_sampler_sample(m_sampler, m_ctx, -1);
        llama_sampler_accept(m_sampler, tok);

        if (llama_vocab_is_eog(vocab, tok)) {
            break;
        }

        char piece[32] = {};
        int n = llama_token_to_piece(vocab, tok, piece, static_cast<int32_t>(sizeof(piece)) - 1, 0, true);
        if (n > 0 && rpos + n < 1023) {
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

    fprintf(stderr, "[VLM] Raw response (%d tokens): %s\n", rpos, response);
    parseVerdictJson(response, verdict, reason, reasonLen);
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void VlmInferenceEngine::parseVerdictJson(const char* json, Orion::TriagePriority& verdict, char* reason,
                                          FwSizeType reasonLen) {
    verdict = TriagePriority::LOW;

    // Search the whole response for category keywords (case-insensitive position)
    if (::strstr(json, "\"HIGH\"") || ::strstr(json, "\"high\""))
        verdict = TriagePriority::HIGH;
    else if (::strstr(json, "\"MEDIUM\"") || ::strstr(json, "\"medium\""))
        verdict = TriagePriority::MEDIUM;

    // Extract the reason value. The model outputs JSON like:
    // {"reason": "some text here", "category": "LOW"}
    // We find "reason" key, then extract the string value by finding
    // the opening quote after ':' and scanning for the closing quote
    // while skipping escaped quotes.
    const char* rp = ::strstr(json, "\"reason\"");
    if (rp) {
        const char* colon = ::strchr(rp, ':');
        if (colon) {
            const char* start = ::strchr(colon, '"');
            if (start) {
                ++start;  // skip opening quote
                // Find closing quote, handling escaped quotes
                const char* p = start;
                while (*p && !(*p == '"' && *(p - 1) != '\\')) {
                    p++;
                }
                if (*p == '"') {
                    FwSizeType len = static_cast<FwSizeType>(p - start);
                    if (len >= reasonLen) len = reasonLen - 1;
                    ::memcpy(reason, start, len);
                    reason[len] = '\0';
                    return;
                }
                // No closing quote found — take everything until end
                FwSizeType len = static_cast<FwSizeType>(::strlen(start));
                if (len >= reasonLen) len = reasonLen - 1;
                ::memcpy(reason, start, len);
                reason[len] = '\0';
                return;
            }
        }
    }

    // No "reason" key found — use the whole response as the reason
    FwSizeType len = static_cast<FwSizeType>(::strlen(json));
    if (len >= reasonLen) len = reasonLen - 1;
    if (len > 0) {
        ::memcpy(reason, json, len);
        reason[len] = '\0';
    } else {
        ::strncpy(reason, "Empty model response", static_cast<size_t>(reasonLen) - 1);
        reason[reasonLen - 1] = '\0';
    }
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

bool VlmInferenceEngine::loadModel() {
    if (m_model) {
        return true;  // Already loaded
    }

    fprintf(stderr, "[VLM] Loading model from: %s\n", getGgufPath());

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;

    m_model = llama_model_load_from_file(getGgufPath(), mparams);
    if (!m_model) {
        fprintf(stderr, "[VLM] llama_model_load_from_file FAILED\n");
        this->log_WARNING_HI_ModelLoadFailed(Fw::String(getGgufPath()));
        return false;
    }
    fprintf(stderr, "[VLM] Text model loaded OK\n");

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = static_cast<uint32_t>(N_CTX);
    cparams.n_batch = static_cast<uint32_t>(N_BATCH);
    cparams.n_threads = N_THREADS;

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        this->log_WARNING_HI_ModelLoadFailed(Fw::String(getGgufPath()));
        return false;
    }

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
        return false;
    }

    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    m_sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(m_sampler, llama_sampler_init_greedy());

    this->log_ACTIVITY_HI_ModelLoaded();
    return true;
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
