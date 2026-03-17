#include "ai_client.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "AI_CLIENT";

// System prompt template — tool descriptions are appended at runtime.
static const char* BASE_SYSTEM_PROMPT =
    "You are a helpful AI assistant running on an ESP32-S3 smart device. "
    "You can call tools to perform actions. When you want to use a tool, "
    "respond ONLY with valid JSON in this format (no other text):\n"
    "{\"tool_call\":{\"name\":\"<tool_name>\",\"parameters\":{<params>}}}\n"
    "When you have a final answer for the user, respond in plain text.\n\n"
    "Available tools:\n";

AIClient::AIClient()
    : m_ollama(nullptr)
    , m_memory(nullptr)
    , m_goals(nullptr)
    , m_tools(nullptr)
    , m_stt(nullptr)
    , m_tts(nullptr)
    , m_responseType(AI_RESPONSE_TEXT)
    , m_initialized(false)
    , m_agentReady(false)
{
}

AIClient::~AIClient()
{
    deinit();
}

esp_err_t AIClient::init()
{
    if (m_initialized) return ESP_OK;

    m_ollama = new OllamaClient();
    m_memory = new MemoryManager();
    m_goals  = new GoalManager();
    m_tools  = new ToolRegistry();

    if (!m_ollama || !m_memory || !m_goals || !m_tools) {
        ESP_LOGE(TAG, "Out of memory allocating agent components");
        deinit();
        return ESP_ERR_NO_MEM;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "AIClient initialized (call initAgent() to connect to Ollama)");
    return ESP_OK;
}

esp_err_t AIClient::initAgent(const char*  ollamaUrl,
                               const char*  model,
                               const char*  sttUrl,
                               const char*  ttsUrl,
                               AudioDriver* audioDriver)
{
    if (!m_initialized) {
        esp_err_t err = init();
        if (err != ESP_OK) return err;
    }

    // Initialize Ollama client
    m_ollama->setServerUrl(ollamaUrl ? ollamaUrl : "http://127.0.0.1:11434");
    m_ollama->setModel(model ? model : "llama2");
    esp_err_t err = m_ollama->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OllamaClient init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Initialize memory (NVS)
    err = m_memory->init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MemoryManager init failed (%s) — continuing without persistence",
                 esp_err_to_name(err));
        // Non-fatal — continue without NVS persistence
    } else {
        m_memory->loadHistory(); // Load any saved conversation history
    }

    // Initialize goal manager
    err = m_goals->init(m_memory);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GoalManager init failed: %s", esp_err_to_name(err));
    }

    // Register built-in tools
    m_tools->registerBuiltinTools();

    // Initialize STT client (optional)
    if (sttUrl) {
        m_stt = new STTClient();
        if (!m_stt) {
            ESP_LOGE(TAG, "Out of memory for STTClient");
            return ESP_ERR_NO_MEM;
        }
        m_stt->setServerUrl(sttUrl);
        err = m_stt->init();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "STTClient init failed (%s) — voice input disabled",
                     esp_err_to_name(err));
            delete m_stt;
            m_stt = nullptr;
        } else {
            ESP_LOGI(TAG, "STTClient ready — url: %s", sttUrl);
        }
    }

    // Initialize TTS client (optional)
    if (ttsUrl && audioDriver) {
        m_tts = new TTSClient();
        if (!m_tts) {
            ESP_LOGE(TAG, "Out of memory for TTSClient");
            return ESP_ERR_NO_MEM;
        }
        m_tts->setServerUrl(ttsUrl);
        err = m_tts->init(audioDriver);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "TTSClient init failed (%s) — voice output disabled",
                     esp_err_to_name(err));
            delete m_tts;
            m_tts = nullptr;
        } else {
            ESP_LOGI(TAG, "TTSClient ready — url: %s", ttsUrl);
        }
    }

    m_agentReady = true;
    ESP_LOGI(TAG, "Agent stack ready — Ollama: %s  model: %s  stt: %s  tts: %s",
             ollamaUrl ? ollamaUrl : "127.0.0.1:11434",
             model     ? model     : "llama2",
             sttUrl    ? sttUrl    : "none",
             ttsUrl    ? ttsUrl    : "none");

    // Optional: health check (non-fatal if server not yet reachable)
    if (m_ollama->healthCheck() == ESP_OK) {
        ESP_LOGI(TAG, "Ollama server reachable");
    } else {
        ESP_LOGW(TAG, "Ollama server not reachable — requests will be retried on demand");
    }

    return ESP_OK;
}

std::string AIClient::buildSystemPrompt() const
{
    std::string prompt = BASE_SYSTEM_PROMPT;
    if (m_tools) {
        prompt += m_tools->getToolDescriptionsForPrompt();
    }
    return prompt;
}

esp_err_t AIClient::processAgentQuery(const char* userInput, std::string& response)
{
    if (!m_initialized || !userInput) return ESP_ERR_INVALID_ARG;

    if (!m_agentReady) {
        ESP_LOGW(TAG, "Agent not fully initialized — call initAgent() first");
        response = "Agent not ready. Please check Ollama server connection.";
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Agent query: %s", userInput);

    // Build system prompt with tool descriptions
    std::string systemPrompt = buildSystemPrompt();

    // Set (or refresh) the system prompt in conversation history
    m_ollama->addSystemPrompt(systemPrompt.c_str());

    // Add user input to persistent memory
    if (m_memory) m_memory->addTurn("user", userInput);

    // Agent loop: up to MAX_TOOL_ITERATIONS tool calls before forcing a final answer
    std::string llmResponse;
    std::string currentInput = userInput;
    int iteration = 0;

    while (iteration < MAX_TOOL_ITERATIONS) {
        ++iteration;

        esp_err_t err = m_ollama->createChatCompletion(currentInput.c_str(), llmResponse);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LLM request failed: %s", esp_err_to_name(err));
            response = "Error communicating with AI server.";
            m_responseType = AI_RESPONSE_TEXT;
            return err;
        }

        // Check if the LLM wants to call a tool
        if (m_tools && m_tools->hasToolCall(llmResponse.c_str())) {
            m_responseType = AI_RESPONSE_TOOL_CALL;
            std::string toolName   = m_tools->extractToolName(llmResponse.c_str());
            std::string toolParams = m_tools->extractToolParams(llmResponse.c_str());

            ESP_LOGI(TAG, "Tool call: %s  params: %s",
                     toolName.c_str(), toolParams.c_str());

            ToolResult result = m_tools->executeTool(toolName.c_str(), toolParams.c_str());

            // Build tool result message to feed back to the LLM
            std::string toolResultMsg;
            if (result.success) {
                toolResultMsg = "Tool '" + toolName + "' result: " + result.output;
            } else {
                toolResultMsg = "Tool '" + toolName + "' failed: " + result.error;
            }

            ESP_LOGI(TAG, "Tool result: %s", toolResultMsg.c_str());

            // Record tool result in persistent memory for tracking.
            // Do NOT call m_ollama->addToHistory here — createChatCompletion will
            // append the tool result as a "user" turn in buildMessagesJson automatically.
            if (m_memory) m_memory->addTurn("tool", toolResultMsg.c_str());

            // Next iteration: feed the tool result back to the LLM as the new prompt
            currentInput = toolResultMsg;
        } else {
            // Final text response — no more tool calls
            break;
        }
    }

    response = llmResponse;
    m_lastResponse = llmResponse;
    m_responseType = AI_RESPONSE_TEXT;

    // Save assistant response to memory
    if (m_memory) {
        m_memory->addTurn("assistant", llmResponse.c_str());
        m_memory->saveHistory(); // Persist (best-effort)
    }

    ESP_LOGI(TAG, "Agent response (%u chars): %.80s%s",
             (unsigned)response.size(), response.c_str(),
             response.size() > 80 ? "..." : "");
    return ESP_OK;
}

esp_err_t AIClient::processVoiceAudio(const int16_t* pcmData,
                                       size_t         numSamples,
                                       int            sampleRate,
                                       std::string&   response)
{
    if (!m_initialized) return ESP_ERR_INVALID_STATE;
    if (!pcmData || numSamples == 0) return ESP_ERR_INVALID_ARG;

    if (!m_stt) {
        ESP_LOGE(TAG, "processVoiceAudio: STTClient not configured");
        return ESP_ERR_INVALID_STATE;
    }

    // Step 1: Speech → Text
    std::string transcript;
    esp_err_t err = m_stt->transcribe(pcmData, numSamples, sampleRate, transcript);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STT transcription failed: %s", esp_err_to_name(err));
        return err;
    }

    if (transcript.empty()) {
        ESP_LOGW(TAG, "STT returned empty transcript — discarding audio");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Voice transcript: %s", transcript.c_str());

    // Step 2: Text → LLM (with full agent tool-call loop)
    err = processAgentQuery(transcript.c_str(), response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Agent query failed: %s", esp_err_to_name(err));
        return err;
    }

    // Step 3: LLM response → TTS (optional)
    if (m_tts && !response.empty()) {
        esp_err_t ttsErr = m_tts->speak(response, /*blocking=*/false);
        if (ttsErr != ESP_OK) {
            // Non-fatal — log and continue; the text response is still valid.
            ESP_LOGW(TAG, "TTS speak failed: %s", esp_err_to_name(ttsErr));
        }
    }

    return ESP_OK;
}

esp_err_t AIClient::processVoiceCommand(const char* command)
{
    if (!m_initialized || !command) return ESP_ERR_INVALID_ARG;

    std::string response;
    esp_err_t err = processAgentQuery(command, response);
    if (err == ESP_OK) {
        m_responseType = AI_RESPONSE_ACTION;
    }
    return err;
}

esp_err_t AIClient::processTextQuery(const char* query)
{
    if (!m_initialized || !query) return ESP_ERR_INVALID_ARG;

    std::string response;
    return processAgentQuery(query, response);
}

const char* AIClient::getResponse()
{
    return m_lastResponse.c_str();
}

ai_response_type_t AIClient::getResponseType()
{
    return m_responseType;
}

void AIClient::deinit()
{
    if (m_ollama) { m_ollama->deinit(); delete m_ollama; m_ollama = nullptr; }
    if (m_memory) { m_memory->deinit(); delete m_memory; m_memory = nullptr; }
    if (m_goals)  { delete m_goals;  m_goals  = nullptr; }
    if (m_tools)  { delete m_tools;  m_tools  = nullptr; }
    if (m_stt)    { m_stt->deinit(); delete m_stt; m_stt = nullptr; }
    if (m_tts)    { m_tts->deinit(); delete m_tts; m_tts = nullptr; }
    m_initialized = false;
    m_agentReady  = false;
}