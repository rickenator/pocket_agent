#include "response_cache.h"
#include "esp_log.h"

static const char* TAG = "RESP_CACHE";

ResponseCache::ResponseCache(size_t maxEntries, int32_t ttlSeconds)
    : m_insertCounter(0)
    , m_mutex(nullptr)
    , m_maxEntries(maxEntries > 0 ? maxEntries : 32)
    , m_ttlUs(ttlSeconds > 0 ? (int64_t)ttlSeconds * 1000000LL : 0)
{
    m_mutex = xSemaphoreCreateMutex();
}

ResponseCache::~ResponseCache()
{
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool ResponseCache::get(const std::string& query, std::string& response)
{
    if (!m_mutex || query.empty()) return false;

    uint32_t hash = hashQuery(query);
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    auto it = m_entries.find(hash);
    // Verify the stored query matches (collision detection)
    if (it == m_entries.end() || it->second.query != query) {
        xSemaphoreGive(m_mutex);
        return false;
    }

    // Check TTL
    if (m_ttlUs > 0) {
        int64_t age = esp_timer_get_time() - it->second.timestampUs;
        if (age > m_ttlUs) {
            // Expired — remove from both maps
            m_insertOrder.erase(it->second.insertSeq);
            m_entries.erase(it);
            xSemaphoreGive(m_mutex);
            ESP_LOGD(TAG, "Cache entry expired for hash 0x%08" PRIx32, hash);
            return false;
        }
    }

    response = it->second.response;
    xSemaphoreGive(m_mutex);
    ESP_LOGD(TAG, "Cache HIT for hash 0x%08" PRIx32, hash);
    return true;
}

void ResponseCache::put(const std::string& query, const std::string& response)
{
    if (!m_mutex || query.empty() || response.empty()) return;

    uint32_t hash = hashQuery(query);
    int64_t  now  = esp_timer_get_time();

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    // If the hash already exists and the query matches, update in place
    auto it = m_entries.find(hash);
    if (it != m_entries.end() && it->second.query == query) {
        m_insertOrder.erase(it->second.insertSeq);
        uint64_t seq          = ++m_insertCounter;
        it->second.response    = response;
        it->second.timestampUs = now;
        it->second.insertSeq   = seq;
        m_insertOrder[seq]     = hash;
        xSemaphoreGive(m_mutex);
        return;
    }

    // Hash collision: if there is a different query at this hash, evict it
    if (it != m_entries.end()) {
        ESP_LOGD(TAG, "Hash collision evicting existing entry for hash 0x%08" PRIx32, hash);
        m_insertOrder.erase(it->second.insertSeq);
        m_entries.erase(it);
    }

    // Evict oldest entry if at capacity
    while (m_entries.size() >= m_maxEntries && !m_insertOrder.empty()) {
        auto oldest = m_insertOrder.begin();
        m_entries.erase(oldest->second);
        m_insertOrder.erase(oldest);
    }

    uint64_t seq       = ++m_insertCounter;
    CacheEntry entry;
    entry.query        = query;
    entry.response     = response;
    entry.timestampUs  = now;
    entry.insertSeq    = seq;
    m_entries[hash]    = entry;
    m_insertOrder[seq] = hash;

    ESP_LOGD(TAG, "Cache PUT hash 0x%08" PRIx32 " (%u entries)", hash, (unsigned)m_entries.size());
    xSemaphoreGive(m_mutex);
}

void ResponseCache::clear()
{
    if (!m_mutex) return;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_entries.clear();
    m_insertOrder.clear();
    xSemaphoreGive(m_mutex);
    ESP_LOGI(TAG, "Cache cleared");
}

size_t ResponseCache::size() const
{
    return m_entries.size();
}

// djb2 hash
uint32_t ResponseCache::hashQuery(const std::string& query)
{
    uint32_t hash = 5381;
    for (unsigned char c : query) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}
