#ifndef RESPONSE_CACHE_H
#define RESPONSE_CACHE_H

#include <string>
#include <map>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

// Simple LRU-style response cache for common/repeated queries.
//
// Stores up to maxEntries cached responses keyed by a hash of the query.
// Hash collisions are handled by storing the full query and verifying on lookup.
// Entries older than ttlSeconds are treated as expired and evicted on lookup.
//
// Usage:
//   ResponseCache cache(32, 300); // 32 entries, 5-minute TTL
//   std::string cached;
//   if (cache.get(query, cached)) {
//       response = cached;
//   } else {
//       // perform LLM call
//       cache.put(query, response);
//   }
class ResponseCache {
public:
    // maxEntries  — maximum number of cached entries (oldest evicted first)
    // ttlSeconds  — seconds before a cached entry expires (0 = never)
    explicit ResponseCache(size_t maxEntries = 32, int32_t ttlSeconds = 300);
    ~ResponseCache();

    // Look up a cached response for the given query.
    // Returns true and populates 'response' on a valid (non-expired) hit.
    bool get(const std::string& query, std::string& response);

    // Store a query→response pair in the cache.
    void put(const std::string& query, const std::string& response);

    // Remove all entries.
    void clear();

    size_t size() const;

private:
    struct CacheEntry {
        std::string query;        // full query, for collision detection
        std::string response;
        int64_t     timestampUs;  // esp_timer_get_time() at insertion
        uint64_t    insertSeq;    // monotonic insertion sequence number
    };

    // Primary map: hash → CacheEntry (with full query for collision detection)
    std::map<uint32_t, CacheEntry> m_entries;
    // Eviction order: insertSeq → hash (monotonic, no timestamp collision risk)
    std::map<uint64_t, uint32_t>   m_insertOrder;
    uint64_t m_insertCounter;

    SemaphoreHandle_t m_mutex;
    size_t   m_maxEntries;
    int64_t  m_ttlUs; // TTL in microseconds (0 = never expire)

    // djb2 hash of the query string (fast, good distribution for short strings)
    static uint32_t hashQuery(const std::string& query);
};

#endif // RESPONSE_CACHE_H
