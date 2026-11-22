#include "avdec.cpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

int main(int argc, char **argv)
{
    using namespace movian::avdec;
    using namespace std::chrono_literals;
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<char> media{std::istreambuf_iterator<char>(input), {}};
    constexpr unsigned span = 512 * 1024;
    movian::torrent::VerifiedFile cache;
    assert(cache.open_cache(media.size(), span));
    std::uint32_t index;
    assert(cache.claim_piece(nullptr, 0, index) && index == 0);
    assert(cache.publish_piece(index, media.data(), span) && !cache.can_start_mkv());
    assert(cache.claim_piece(nullptr, 0, index) && index == 1);
    assert(cache.publish_piece(index, media.data() + span, span) && cache.can_start_mkv());
    auto *buffer = static_cast<std::uint8_t *>(av_malloc(32768));
    g_torrent_input = &cache;
    g_torrent_io = avio_alloc_context(buffer, 32768, 0, nullptr, torrent_read, nullptr, torrent_seek);
    g_fmt = avformat_alloc_context();
    g_fmt->pb = g_torrent_io;
    g_fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
    assert(avformat_open_input(&g_fmt, "fixture.mkv", av_find_input_format("matroska"), nullptr) == 0);
    assert(g_fmt->nb_streams >= 1);
    auto *reference = static_cast<AVFormatContext *>(nullptr);
    assert(avformat_open_input(&reference, argv[1], av_find_input_format("matroska"), nullptr) == 0);
    auto *expected = av_packet_alloc();
    g_pkt = av_packet_alloc();
    g_run = true;
    std::atomic<bool> publishing{false};
    std::thread writer([&] {
        for (int i = 0; i < 300 && !g_peer_buffering; ++i)
            std::this_thread::sleep_for(5ms);
        assert(g_peer_buffering);
        publishing = true;
        std::uint32_t n;
        while (cache.claim_piece(nullptr, 0, n))
        {
            const auto offset = std::size_t(n) * span;
            assert(cache.publish_piece(n, media.data() + offset, std::min(media.size() - offset, std::size_t(span))));
        }
    });
    int early = 0, packets = 0, rc;
    while (true)
    {
        pthread_mutex_lock(&g_lock);
        rc = read_packet();
        pthread_mutex_unlock(&g_lock);
        if (rc < 0) break;
        assert(av_read_frame(reference, expected) == 0 && g_pkt->size == expected->size);
        assert(g_pkt->stream_index == expected->stream_index);
        assert(!std::memcmp(g_pkt->data, expected->data, g_pkt->size));
        early += !publishing;
        ++packets;
        av_packet_unref(g_pkt);
        av_packet_unref(expected);
    }
    writer.join();
    assert(early > 0 && rc == AVERROR_EOF && av_read_frame(reference, expected) == AVERROR_EOF);
    assert(av_seek_frame(g_fmt, -1, 2000000, AVSEEK_FLAG_BACKWARD) >= 0);
    assert(av_seek_frame(reference, -1, 2000000, AVSEEK_FLAG_BACKWARD) >= 0);
    assert(av_read_frame(g_fmt, g_pkt) == 0 && av_read_frame(reference, expected) == 0);
    assert(g_pkt->pts == expected->pts && g_pkt->size == expected->size);
    std::printf("Matroska opened with two verified pieces, played %d early packets, matched %d through EOF and sought correctly\n", early, packets);
    av_packet_free(&expected);
    av_packet_free(&g_pkt);
    avformat_close_input(&reference);
    avformat_close_input(&g_fmt);
    av_freep(&g_torrent_io->buffer);
    avio_context_free(&g_torrent_io);
}
