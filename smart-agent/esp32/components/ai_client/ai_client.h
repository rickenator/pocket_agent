#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include <stdint.h>
#include <string>
#include "esp_err.h"
#include "ollama_client.h"
#include "memory_manager.h"
#include "goal_manager.h"
#include "tool_registry.h"
#include "circuit_breaker.h"
#include "response_cache.h"
#include "../stt_client/stt_client.h"
#include "../tts_client/tts_client.h"

// AI response types
typedef enum {
    AI_RESPONSE_TEXT = 0,
    AI_RESPONSE_ACTION,
    AI_RESPONSE_VOICE,
    AI_RESPONSE_TOOL_CALL,
} ai_response_type_t;

// AI client — acts as the top-level agent orchestrator.
//
// Full speech-to-response pipeline (ESP32 mode):
//
//   I2S mic  →  VoiceDriver (PCM)
//                   │
//                   ▼
//              STTClient ──── POST audio ──► Whisper / Google STT
//                   │                         (e.g. 192.168.1.251:9000)
//                   │  transcript (text)
//                   ▼
//              OllamaClient ── POST /api/chat ─► Ollama LLM server
//                   │                             (e.g. 192.168.1.251:11434)
//                   │  LLM response (text)
//                   ▼
//              TTSClient ──── POST text ──────► Piper / Google TTS
//                   │                            (e.g. 192.168.1.251:5000)
//                   │  WAV audio
//                   ▼
//              AudioDriver  →  I2S speaker
class AIClient {
public:
    AIClient();
    ~AIClient();

    esp_err_t init();
    void deinit();

    // Full agent initialisation: connects Ollama, memory, goals, tools, STT and TTS.
    //   ollamaUrl  — e.g. "http://192.168.1.251:11434"
    //   model      — e.g. "llama2" or "mistral"
    //   sttUrl     — e.g. "http://192.168.1.251:9000"  (pass nullptr to skip STT)
    //   ttsUrl     — e.g. "http://192.168.1.251:5000"  (pass nullptr to skip TTS)
    //   audioDriver — AudioDriver instance for I2S speaker output (may be nullptr)
    esp_err_t initAgent(const char*  ollamaUrl,
                        const char*  model,
                        const char*  sttUrl      = nullptr,
                        const char*  ttsUrl      = nullptr,
                        AudioDriver* audioDriver = nullptr);

    // Agent query: implements the full tool-call loop
    //   1. Add userInput to memory
    //   2. Build system prompt with tool descriptions
    //   3. Send to Ollama (with conversation history)
    //   4. If LLM returns a tool call, execute tool and re-query
    //   5. Repeat until final text response; save to memory
    //   Responses for identical queries are served from the response cache.
    esp_err_t processAgentQuery(const char* userInput, std::string& response);

    // Streaming variant: calls responseCallback for each response chunk.
    // TTS can begin consuming chunks immediately for reduced perceived latency.
    // Note: tool-call iterations are performed internally before streaming begins.
    esp_err_t processAgentQueryStream(const char* userInput,
                                      ollama_stream_callback_t responseCallback);

    // Full voice pipeline: PCM audio → STT → Ollama → TTS → I2S speaker.
    //   pcmData    — signed 16-bit PCM samples (mono)
    //   numSamples — number of samples in pcmData
    //   sampleRate — capture sample rate in Hz (e.g. 16 000)
    //   response   — [out] LLM text response; also spoken via TTSClient if configured
    //
    // Returns ESP_OK on success; any sub-step failure propagates its esp_err_t.
    esp_err_t processVoiceAudio(const int16_t* pcmData,
                                size_t         numSamples,
                                int            sampleRate,
                                std::string&   response);

    // Legacy interface — delegates to processAgentQuery
    esp_err_t processVoiceCommand(const char* command);
    esp_err_t processTextQuery(const char* query);

    // Get last response (legacy callers)
    const char* getResponse();
    ai_response_type_t getResponseType();

    // Accessors for sub-components
    OllamaClient*  getOllama()   { return m_ollama;   }
    MemoryManager* getMemory()   { return m_memory;   }
    GoalManager*   getGoals()    { return m_goals;    }
    ToolRegistry*  getTools()    { return m_tools;    }
    STTClient*     getSTT()      { return m_stt;      }
    TTSClient*     getTTS()      { return m_tts;      }
    CircuitBreaker* getCircuitBreaker() { return &m_circuitBreaker; }
    ResponseCache* getCache()    { return &m_cache;   }

private:
    OllamaClient*  m_ollama;
    MemoryManager* m_memory;
    GoalManager*   m_goals;
    ToolRegistry*  m_tools;
    STTClient*     m_stt;
    TTSClient*     m_tts;

    CircuitBreaker m_circuitBreaker;
    ResponseCache  m_cache;

    std::string        m_lastResponse;
    ai_response_type_t m_responseType;
    bool               m_initialized;
    bool               m_agentReady;

    // Default maximum tool iterations (may be overridden adaptively).
    static const int MAX_TOOL_ITERATIONS = 5;

    std::string buildSystemPrompt() const;

    // Returns an adaptive iteration limit based on keywords in userInput.
    static int calculateMaxIterations(const std::string& userInput);
};

#endif // AI_CLIENT_H