#ifndef RECORDING_STREAMER_H
#define RECORDING_STREAMER_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

// PCM format constants
#define REC_SAMPLE_RATE     16000
#define REC_SAMPLE_WIDTH    2       // 16-bit
#define REC_CHANNELS        1       // mono
#define REC_BYTES_PER_SEC   (REC_SAMPLE_RATE * REC_SAMPLE_WIDTH * REC_CHANNELS)  // 32000
#define REC_CHUNK_SIZE      3200    // 100ms of audio per UDP packet

// Protocol header prepended to each UDP packet (for session management)
#pragma pack(push, 1)
struct RecPacketHeader {
    uint8_t  magic;        // 0x52 'R'
    uint8_t  seq_hi;       // sequence number high byte
    uint16_t seq_lo;       // sequence number low byte
    uint32_t timestamp_ms; // milliseconds since recording start
};
#pragma pack(pop)

static_assert(sizeof(RecPacketHeader) == 8, "RecPacketHeader must be 8 bytes");

class RecordingStreamer {
public:
    RecordingStreamer() = default;
    ~RecordingStreamer();

    // Configure the streaming target
    void SetServer(const std::string& host, int port);
    void SetServerIp(const std::string& ip, int port);

    // Start/stop the streaming task
    bool Start();
    void Stop();

    // Feed PCM data from the audio input task (thread-safe)
    // Call this with raw 16-bit PCM samples. Data is queued and sent by the streamer task.
    void PushPcm(const int16_t* data, int samples);

    // Query state
    bool IsRunning() const { return running_.load(); }
    uint32_t GetElapsedMs() const { return elapsed_ms_.load(); }
    uint32_t GetBytesSent() const { return bytes_sent_.load(); }
    uint32_t GetPacketsDropped() const { return packets_dropped_.load(); }

    // Get a summary string for display
    std::string GetStatusString() const;

private:
    void StreamerTask();

    // Configuration
    std::string server_ip_;
    int server_port_ = 10096;
    struct sockaddr_in server_addr_ = {};

    // Runtime state
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<uint32_t> elapsed_ms_{0};
    std::atomic<uint32_t> bytes_sent_{0};
    std::atomic<uint32_t> packets_dropped_{0};
    std::atomic<uint32_t> packet_seq_{0};

    // PCM ring buffer (lock-free single-producer single-consumer)
    static constexpr size_t kBufferFrames = 8;  // 8 x 3200 bytes = 25.6KB
    static constexpr size_t kFrameBytes = REC_CHUNK_SIZE;
    alignas(64) std::atomic<uint32_t> write_idx_{0};
    alignas(64) std::atomic<uint32_t> read_idx_{0};
    int16_t pcm_buffer_[kBufferFrames][REC_CHUNK_SIZE / REC_SAMPLE_WIDTH];

    // UDP socket
    int socket_fd_ = -1;
    TaskHandle_t task_handle_ = nullptr;
};

#endif // RECORDING_STREAMER_H
