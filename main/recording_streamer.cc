#include "recording_streamer.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "RecStreamer"

RecordingStreamer::~RecordingStreamer() {
    Stop();
}

void RecordingStreamer::SetServer(const std::string& host, int port) {
    server_ip_ = host;
    server_port_ = port;

    struct hostent* he = gethostbyname(host.c_str());
    if (he && he->h_addr_list[0]) {
        memcpy(&server_addr_.sin_addr, he->h_addr_list[0], sizeof(server_addr_.sin_addr));
    } else {
        // Assume it's already an IP string
        inet_aton(host.c_str(), &server_addr_.sin_addr);
    }
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    ESP_LOGI(TAG, "Server set: %s:%d", host.c_str(), port);
}

void RecordingStreamer::SetServerIp(const std::string& ip, int port) {
    server_ip_ = ip;
    server_port_ = port;
    inet_aton(ip.c_str(), &server_addr_.sin_addr);
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    ESP_LOGI(TAG, "Server IP set: %s:%d", ip.c_str(), port);
}

bool RecordingStreamer::Start() {
    if (running_.load()) {
        ESP_LOGW(TAG, "Already running");
        return false;
    }

    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        return false;
    }

    // Reset state
    elapsed_ms_.store(0);
    bytes_sent_.store(0);
    packets_dropped_.store(0);
    packet_seq_.store(0);
    write_idx_.store(0);
    read_idx_.store(0);
    stop_requested_.store(false);
    running_.store(true);

    // Start the streamer task (priority 12, above audio input at 10)
    xTaskCreate(
        [](void* arg) {
            auto* self = static_cast<RecordingStreamer*>(arg);
            self->StreamerTask();
        },
        "rec_stream", 4096, this, 12, &task_handle_
    );

    ESP_LOGI(TAG, "Recording streamer started → %s:%d", server_ip_.c_str(), server_port_);
    return true;
}

void RecordingStreamer::Stop() {
    if (!running_.load()) return;

    stop_requested_.store(true);
    running_.store(false);

    // Wait for task to finish (max 500ms)
    int waits = 0;
    while (task_handle_ && waits < 50) {
        vTaskDelay(pdMS_TO_TICKS(10));
        waits++;
    }
    task_handle_ = nullptr;

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }

    ESP_LOGI(TAG, "Recording stopped. Bytes sent: %u, Dropped: %u",
             bytes_sent_.load(), packets_dropped_.load());
}

void RecordingStreamer::PushPcm(const int16_t* data, int samples) {
    if (!running_.load()) return;

    int bytes_available = samples * REC_SAMPLE_WIDTH;
    int offset = 0;

    while (bytes_available >= REC_CHUNK_SIZE) {
        uint32_t idx = write_idx_.load();
        uint32_t next = (idx + 1) % kBufferFrames;

        // Check if buffer is full
        if (next == read_idx_.load()) {
            // Buffer full - drop packet
            packets_dropped_.fetch_add(1);
            break;
        }

        // Copy PCM data into ring buffer slot
        memcpy(pcm_buffer_[idx], data + offset / REC_SAMPLE_WIDTH, REC_CHUNK_SIZE);
        write_idx_.store(next);

        offset += REC_CHUNK_SIZE;
        bytes_available -= REC_CHUNK_SIZE;
    }
}

void RecordingStreamer::StreamerTask() {
    int64_t start_time_us = esp_timer_get_time();
    uint8_t pkt_buf[sizeof(RecPacketHeader) + REC_CHUNK_SIZE];
    auto* header = reinterpret_cast<RecPacketHeader*>(pkt_buf);
    auto* pcm_data = reinterpret_cast<int16_t*>(pkt_buf + sizeof(RecPacketHeader));

    while (!stop_requested_.load()) {
        uint32_t idx = read_idx_.load();
        if (idx == write_idx_.load()) {
            // No data available - small delay to avoid busy-wait
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Read PCM from ring buffer
        memcpy(pcm_data, pcm_buffer_[idx], REC_CHUNK_SIZE);
        read_idx_.store((idx + 1) % kBufferFrames);

        // Fill header
        uint32_t seq = packet_seq_.fetch_add(1);
        header->magic = 0x52;
        header->seq_hi = (seq >> 8) & 0xFF;
        header->seq_lo = seq & 0xFF;
        header->timestamp_ms = (uint32_t)((esp_timer_get_time() - start_time_us) / 1000);

        // Send via UDP
        int sent = sendto(socket_fd_, pkt_buf, sizeof(RecPacketHeader) + REC_CHUNK_SIZE,
                          0, (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if (sent > 0) {
            bytes_sent_.fetch_add(REC_CHUNK_SIZE);
        } else {
            packets_dropped_.fetch_add(1);
        }

        // Update elapsed time
        elapsed_ms_.store((uint32_t)((esp_timer_get_time() - start_time_us) / 1000));
    }

    // Task exit
    if (task_handle_) {
        vTaskDelete(nullptr);
    }
}

std::string RecordingStreamer::GetStatusString() const {
    char buf[64];
    uint32_t ms = elapsed_ms_.load();
    int min = ms / 60000;
    int sec = (ms % 60000) / 1000;
    snprintf(buf, sizeof(buf), "REC %d:%02d", min, sec);
    return std::string(buf);
}
