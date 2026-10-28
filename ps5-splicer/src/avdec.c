#include "avdec.h"
#include "hwdec.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

#include <stdlib.h>
#include <string.h>

struct avdec {
	AVFormatContext *fmt;
	AVCodecContext *codec;
	AVPacket *pkt;
	AVFrame *frame;
	uint8_t *rgba;
	int vindex;
	int w, h;
	int eof;
	double clock;
	double pts;
	double duration;
	AVRational tb;
	enum AVPixelFormat pix;
	hwdec_t *hw;
	int nal_size;
	int sent_prefix;
	uint8_t *prefix;
	int prefix_n;
	uint8_t *scratch;
	AVCodecContext *acodec;
	AVFrame *aframe;
	int aindex;
	float *pcm;
	int pcm_cap;
	avdec_pcm_fn on_pcm;
	void *pcm_user;
};

static int
clip8(int v)
{
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return v;
}

static void
put_rgb(uint8_t *p, int y, int u, int v, int full)
{
	int c, d, e, r, g, b;
	if (full) {
		c = y;
		d = u - 128;
		e = v - 128;
		r = (256 * c + 359 * e) >> 8;
		g = (256 * c - 88 * d - 183 * e) >> 8;
		b = (256 * c + 454 * d) >> 8;
	} else {
		c = y - 16;
		d = u - 128;
		e = v - 128;
		r = (298 * c + 409 * e + 128) >> 8;
		g = (298 * c - 100 * d - 208 * e + 128) >> 8;
		b = (298 * c + 516 * d + 128) >> 8;
	}
	p[0] = (uint8_t)clip8(r);
	p[1] = (uint8_t)clip8(g);
	p[2] = (uint8_t)clip8(b);
	p[3] = 255;
}

static int
convert(avdec_t *a)
{
	AVFrame *f = a->frame;
	int x, y, w = a->w, h = a->h;
	uint8_t *dst = a->rgba;
	if (!dst || f->width < w || f->height < h)
		return -1;
	if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P) {
		int full = f->format == AV_PIX_FMT_YUVJ420P;
		for (y = 0; y < h; y++) {
			const uint8_t *yp = f->data[0] + (size_t)y * f->linesize[0];
			const uint8_t *up = f->data[1] + (size_t)(y / 2) * f->linesize[1];
			const uint8_t *vp = f->data[2] + (size_t)(y / 2) * f->linesize[2];
			for (x = 0; x < w; x++) {
				put_rgb(dst, yp[x], up[x / 2], vp[x / 2], full);
				dst += 4;
			}
		}
		return 0;
	}
	if (f->format == AV_PIX_FMT_NV12) {
		for (y = 0; y < h; y++) {
			const uint8_t *yp = f->data[0] + (size_t)y * f->linesize[0];
			const uint8_t *uv = f->data[1] + (size_t)(y / 2) * f->linesize[1];
			for (x = 0; x < w; x++) {
				put_rgb(dst, yp[x], uv[(x & ~1)], uv[(x & ~1) + 1], 0);
				dst += 4;
			}
		}
		return 0;
	}
	if (f->format == AV_PIX_FMT_YUV420P10LE) {
		for (y = 0; y < h; y++) {
			const uint16_t *yp = (const uint16_t *)(f->data[0] + (size_t)y * f->linesize[0]);
			const uint16_t *up = (const uint16_t *)(f->data[1] + (size_t)(y / 2) * f->linesize[1]);
			const uint16_t *vp = (const uint16_t *)(f->data[2] + (size_t)(y / 2) * f->linesize[2]);
			for (x = 0; x < w; x++) {
				put_rgb(dst, yp[x] >> 2, up[x / 2] >> 2, vp[x / 2] >> 2, 0);
				dst += 4;
			}
		}
		return 0;
	}
	return -1;
}

static double
frame_pts(avdec_t *a)
{
	int64_t p = a->frame->best_effort_timestamp;
	if (p == AV_NOPTS_VALUE)
		p = a->frame->pts;
	if (p == AV_NOPTS_VALUE)
		return a->pts + 1.0 / 30.0;
	return av_q2d(a->tb) * (double)p;
}

static int
is_audio_id(enum AVCodecID id)
{
	switch (id) {
	case AV_CODEC_ID_AAC:
	case AV_CODEC_ID_AAC_LATM:
	case AV_CODEC_ID_AC3:
	case AV_CODEC_ID_EAC3:
	case AV_CODEC_ID_MP3:
	case AV_CODEC_ID_DTS:
	case AV_CODEC_ID_TRUEHD:
	case AV_CODEC_ID_MLP:
	case AV_CODEC_ID_OPUS:
	case AV_CODEC_ID_FLAC:
	case AV_CODEC_ID_PCM_S16LE:
	case AV_CODEC_ID_PCM_S16BE:
	case AV_CODEC_ID_PCM_S24LE:
	case AV_CODEC_ID_PCM_S24BE:
	case AV_CODEC_ID_PCM_S32LE:
	case AV_CODEC_ID_PCM_S32BE:
	case AV_CODEC_ID_PCM_F32LE:
	case AV_CODEC_ID_PCM_F32BE:
	case AV_CODEC_ID_PCM_U8:
	case AV_CODEC_ID_PCM_BLURAY:
	case AV_CODEC_ID_PCM_DVD:
		return 1;
	default:
		return 0;
	}
}

static float
sample_at(const AVFrame *f, int ch, int i)
{
	int nch = f->ch_layout.nb_channels > 0 ? f->ch_layout.nb_channels : 1;
	if (ch < 0 || ch >= nch)
		return 0;
	if (f->format == AV_SAMPLE_FMT_FLTP) {
		const float *p = (const float *)f->extended_data[ch];
		return p ? p[i] : 0;
	}
	if (f->format == AV_SAMPLE_FMT_FLT) {
		const float *p = (const float *)f->extended_data[0];
		return p ? p[i * nch + ch] : 0;
	}
	if (f->format == AV_SAMPLE_FMT_S16P) {
		const int16_t *p = (const int16_t *)f->extended_data[ch];
		return p ? (float)p[i] / 32768.0f : 0;
	}
	if (f->format == AV_SAMPLE_FMT_S16) {
		const int16_t *p = (const int16_t *)f->extended_data[0];
		return p ? (float)p[i * nch + ch] / 32768.0f : 0;
	}
	if (f->format == AV_SAMPLE_FMT_S32P) {
		const int32_t *p = (const int32_t *)f->extended_data[ch];
		return p ? (float)p[i] / 2147483648.0f : 0;
	}
	if (f->format == AV_SAMPLE_FMT_S32) {
		const int32_t *p = (const int32_t *)f->extended_data[0];
		return p ? (float)p[i * nch + ch] / 2147483648.0f : 0;
	}
	if (f->format == AV_SAMPLE_FMT_U8P) {
		const uint8_t *p = (const uint8_t *)f->extended_data[ch];
		return p ? ((float)p[i] - 128.0f) / 128.0f : 0;
	}
	if (f->format == AV_SAMPLE_FMT_U8) {
		const uint8_t *p = (const uint8_t *)f->extended_data[0];
		return p ? ((float)p[i * nch + ch] - 128.0f) / 128.0f : 0;
	}
	return 0;
}

static float
clamp1(float s)
{
	if (s > 1.0f)
		return 1.0f;
	if (s < -1.0f)
		return -1.0f;
	return s;
}

static void
emit_audio(avdec_t *a)
{
	AVFrame *f;
	int n, ch, i;
	if (!a->acodec || !a->aframe || !a->on_pcm)
		return;
	f = a->aframe;
	for (;;) {
		int r = avcodec_receive_frame(a->acodec, f);
		if (r)
			return;
		n = f->nb_samples;
		ch = f->ch_layout.nb_channels > 0 ? f->ch_layout.nb_channels : 1;
		if (n > 0) {
			if (n > a->pcm_cap) {
				float *p = (float *)realloc(a->pcm, (size_t)n * 2 * sizeof(float));
				if (!p) {
					av_frame_unref(f);
					return;
				}
				a->pcm = p;
				a->pcm_cap = n;
			}
			for (i = 0; i < n; i++) {
				float L, R;
				if (ch == 1) {
					L = R = sample_at(f, 0, i);
				} else if (ch >= 6) {
					float c = sample_at(f, 2, i);
					float lfe = sample_at(f, 3, i);
					L = sample_at(f, 0, i) + 0.707f * c + 0.707f * sample_at(f, 4, i) + 0.25f * lfe;
					R = sample_at(f, 1, i) + 0.707f * c + 0.707f * sample_at(f, 5, i) + 0.25f * lfe;
					if (ch >= 8) {
						L += 0.5f * sample_at(f, 6, i);
						R += 0.5f * sample_at(f, 7, i);
					}
				} else {
					L = sample_at(f, 0, i);
					R = sample_at(f, 1, i);
					if (ch > 2) {
						float c = sample_at(f, 2, i);
						L += 0.707f * c;
						R += 0.707f * c;
					}
				}
				a->pcm[i * 2] = clamp1(L);
				a->pcm[i * 2 + 1] = clamp1(R);
			}
			a->on_pcm(a->pcm_user, a->pcm, n,
				  f->sample_rate > 0 ? f->sample_rate : 48000);
		}
		av_frame_unref(f);
	}
}

static void
feed_audio_packet(avdec_t *a)
{
	int r;
	if (!a->acodec || !a->pkt || a->aindex < 0 || a->pkt->stream_index != a->aindex)
		return;
	r = avcodec_send_packet(a->acodec, a->pkt);
	if (r == AVERROR(EAGAIN)) {
		emit_audio(a);
		r = avcodec_send_packet(a->acodec, a->pkt);
	}
	if (r == 0)
		emit_audio(a);
}

static int
open_audio(avdec_t *a)
{
	const AVCodec *codec;
	AVStream *st;
	enum AVCodecID id;
	int i;
	a->aindex = -1;
	if (!a->fmt)
		return -1;
	for (i = 0; i < (int)a->fmt->nb_streams; i++) {
		if (a->fmt->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
			continue;
		id = a->fmt->streams[i]->codecpar->codec_id;
		if (!is_audio_id(id))
			continue;
		a->aindex = i;
		break;
	}
	if (a->aindex < 0)
		return -1;
	st = a->fmt->streams[a->aindex];
	codec = avcodec_find_decoder(st->codecpar->codec_id);
	if (!codec)
		goto fail;
	a->acodec = avcodec_alloc_context3(codec);
	a->aframe = av_frame_alloc();
	if (!a->acodec || !a->aframe)
		goto fail;
	if (avcodec_parameters_to_context(a->acodec, st->codecpar) < 0)
		goto fail;
	a->acodec->thread_count = 1;
	if (avcodec_open2(a->acodec, codec, NULL) < 0)
		goto fail;
	return 0;
fail:
	if (a->aframe)
		av_frame_free(&a->aframe);
	if (a->acodec)
		avcodec_free_context(&a->acodec);
	a->aindex = -1;
	return -1;
}

static int
hw_pull(avdec_t *a)
{
	int spins = 0;
	if (a->prefix_n && !a->sent_prefix) {
		a->sent_prefix = 1;
		hwdec_feed(a->hw, a->prefix, a->prefix_n);
	}
	for (;;) {
		int r;
		int n;
		if (++spins > 64)
			return 0;
		if (a->eof) {
			av_seek_frame(a->fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
			hwdec_reset(a->hw);
			a->sent_prefix = 0;
			a->eof = 0;
			a->pts = 0;
			return 0;
		}
		r = av_read_frame(a->fmt, a->pkt);
		if (r < 0) {
			a->eof = 1;
			continue;
		}
		if (a->pkt->stream_index == a->aindex) {
			feed_audio_packet(a);
			av_packet_unref(a->pkt);
			continue;
		}
		if (a->pkt->stream_index != a->vindex) {
			av_packet_unref(a->pkt);
			continue;
		}
		{
			double step = 1.0 / 30.0;
			AVRational rate = a->fmt->streams[a->vindex]->avg_frame_rate;
			if (a->pkt->duration > 0)
				step = av_q2d(a->tb) * (double)a->pkt->duration;
			else if (rate.num > 0 && rate.den > 0)
				step = av_q2d(av_inv_q(rate));
			if (step < 1.0 / 120.0)
				step = 1.0 / 30.0;
			n = hwdec_packet_annexb(a->pkt->data, a->pkt->size, a->nal_size,
						a->scratch, 6 << 20);
			av_packet_unref(a->pkt);
			if (n <= 0)
				continue;
			r = hwdec_feed(a->hw, a->scratch, n);
			if (r < 0)
				return -1;
			if (r > 0) {
				a->pts += step;
				return 1;
			}
		}
	}
}

static int
try_hardware(avdec_t *a, AVStream *st)
{
	AVCodecParameters *par = st->codecpar;
	int hevc = par->codec_id == AV_CODEC_ID_HEVC;
	int ten = par->format == AV_PIX_FMT_YUV420P10LE || par->format == AV_PIX_FMT_P010LE;
	int w = par->width;
	int h = par->height;
	int packets = 0;
	int got = 0;
	if (par->codec_id == AV_CODEC_ID_H264 && ten)
		return -1;
	if (!hwdec_ready())
		return -1;
	if (w < 16 || h < 16 || w > 3840 || h > 2176 || ((w | h) & 1))
		return -1;
	if (!a->pkt)
		return -1;
	a->prefix = (uint8_t *)malloc(64 * 1024);
	a->scratch = (uint8_t *)malloc(6 << 20);
	if (!a->prefix || !a->scratch)
		goto giveup;
	a->prefix_n = hwdec_extradata_annexb(hevc, par->extradata, par->extradata_size,
					     a->prefix, 64 * 1024, &a->nal_size);
	if (a->prefix_n < 0)
		a->prefix_n = 0;
	if (hwdec_open(&a->hw, hevc, ten, w, h) != 0)
		goto giveup;
	a->w = w;
	a->h = h;
	a->rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
	if (!a->rgba)
		goto giveup;
	memset(a->rgba, 0, (size_t)w * (size_t)h * 4);
	hwdec_bind(a->hw, a->rgba, w, h);
	a->tb = st->time_base;
	while (packets < 48 && !got) {
		int n, r;
		if (a->prefix_n && !a->sent_prefix) {
			a->sent_prefix = 1;
			hwdec_feed(a->hw, a->prefix, a->prefix_n);
		}
		r = av_read_frame(a->fmt, a->pkt);
		if (r < 0)
			break;
		if (a->pkt->stream_index != a->vindex) {
			av_packet_unref(a->pkt);
			continue;
		}
		packets++;
		n = hwdec_packet_annexb(a->pkt->data, a->pkt->size, a->nal_size,
					a->scratch, 6 << 20);
		av_packet_unref(a->pkt);
		if (n <= 0)
			continue;
		if (hwdec_feed(a->hw, a->scratch, n) > 0)
			got = 1;
	}
	if (!got)
		goto giveup;
	av_seek_frame(a->fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
	hwdec_reset(a->hw);
	if (a->acodec)
		avcodec_flush_buffers(a->acodec);
	a->sent_prefix = 0;
	a->eof = 0;
	a->pts = 0;
	a->clock = 0;
	return 0;
giveup:
	if (a->hw) {
		hwdec_close(a->hw);
		a->hw = NULL;
	}
	free(a->rgba);
	a->rgba = NULL;
	free(a->prefix);
	a->prefix = NULL;
	free(a->scratch);
	a->scratch = NULL;
	a->prefix_n = 0;
	a->sent_prefix = 0;
	a->nal_size = 0;
	if (a->fmt)
		av_seek_frame(a->fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
	a->eof = 0;
	return -1;
}

static int
pull_one(avdec_t *a)
{
	int r, spins = 0;
	if (a->hw)
		return hw_pull(a);
	if (!a->codec || !a->frame)
		return -1;
	for (;;) {
		if (++spins > 4096)
			return 0;
		r = avcodec_receive_frame(a->codec, a->frame);
		if (r == 0) {
			if (convert(a) != 0) {
				av_frame_unref(a->frame);
				return -1;
			}
			a->pts = frame_pts(a);
			if (a->pts < 0)
				a->pts = 0;
			av_frame_unref(a->frame);
			return 1;
		}
		if (r != AVERROR(EAGAIN))
			return 0;
		if (a->eof)
			return 0;
		r = av_read_frame(a->fmt, a->pkt);
		if (r < 0) {
			a->eof = 1;
			avcodec_send_packet(a->codec, NULL);
			continue;
		}
		if (a->pkt->stream_index == a->aindex)
			feed_audio_packet(a);
		else if (a->pkt->stream_index == a->vindex)
			avcodec_send_packet(a->codec, a->pkt);
		av_packet_unref(a->pkt);
	}
}

int
avdec_open(avdec_t **out, const char *path, uint8_t **rgba, int *w, int *h,
	   double *duration, avdec_pcm_fn pcm, void *pcm_user)
{
	avdec_t *a;
	const AVCodec *codec;
	AVStream *st;
	int i, ret;
	*out = NULL;
	*rgba = NULL;
	a = (avdec_t *)calloc(1, sizeof *a);
	if (!a)
		return -1;
	a->vindex = -1;
	a->aindex = -1;
	a->on_pcm = pcm;
	a->pcm_user = pcm_user;
	if (avformat_open_input(&a->fmt, path, NULL, NULL) < 0)
		goto fail;
	if (avformat_find_stream_info(a->fmt, NULL) < 0)
		goto fail;
	for (i = 0; i < (int)a->fmt->nb_streams; i++) {
		enum AVCodecID id = a->fmt->streams[i]->codecpar->codec_id;
		if (a->fmt->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
			continue;
		if (id == AV_CODEC_ID_H264 || id == AV_CODEC_ID_HEVC) {
			a->vindex = i;
			break;
		}
	}
	if (a->vindex < 0)
		goto fail;
	st = a->fmt->streams[a->vindex];
	a->pkt = av_packet_alloc();
	a->frame = av_frame_alloc();
	if (!a->pkt || !a->frame)
		goto fail;
	open_audio(a);
	if (a->fmt->duration > 0)
		a->duration = (double)a->fmt->duration / (double)AV_TIME_BASE;
	if (try_hardware(a, st) == 0) {
		*rgba = a->rgba;
		*w = a->w;
		*h = a->h;
		*duration = a->duration;
		*out = a;
		return 0;
	}
	codec = avcodec_find_decoder(st->codecpar->codec_id);
	if (!codec)
		goto fail;
	a->codec = avcodec_alloc_context3(codec);
	if (!a->codec)
		goto fail;
	if (avcodec_parameters_to_context(a->codec, st->codecpar) < 0)
		goto fail;
	a->codec->thread_count = 1;
	if (avcodec_open2(a->codec, codec, NULL) < 0)
		goto fail;
	a->w = a->codec->width;
	a->h = a->codec->height;
	if (a->w < 16 || a->h < 16 || a->w > 1920 || a->h > 1088)
		goto fail;
	a->rgba = (uint8_t *)malloc((size_t)a->w * (size_t)a->h * 4);
	if (!a->rgba)
		goto fail;
	memset(a->rgba, 0, (size_t)a->w * (size_t)a->h * 4);
	a->tb = st->time_base;
	a->pix = a->codec->pix_fmt;
	if (a->fmt->duration > 0 && a->duration <= 0)
		a->duration = (double)a->fmt->duration / (double)AV_TIME_BASE;
	ret = pull_one(a);
	if (ret <= 0)
		goto fail;
	a->clock = a->pts;
	*rgba = a->rgba;
	*w = a->w;
	*h = a->h;
	*duration = a->duration;
	*out = a;
	return 0;
fail:
	free(a->rgba);
	a->rgba = NULL;
	avdec_close(a);
	return -1;
}

void
avdec_close(avdec_t *a)
{
	if (!a)
		return;
	if (a->hw)
		hwdec_close(a->hw);
	free(a->prefix);
	free(a->scratch);
	free(a->pcm);
	if (a->aframe)
		av_frame_free(&a->aframe);
	if (a->acodec)
		avcodec_free_context(&a->acodec);
	if (a->frame)
		av_frame_free(&a->frame);
	if (a->pkt)
		av_packet_free(&a->pkt);
	if (a->codec)
		avcodec_free_context(&a->codec);
	if (a->fmt)
		avformat_close_input(&a->fmt);
	free(a);
}

int
avdec_tick(avdec_t *a, double dt)
{
	int n = 0;
	if (!a)
		return 0;
	if (dt < 0)
		dt = 0;
	a->clock += dt;
	while (a->pts + 0.0005 < a->clock && n < 6) {
		int r = pull_one(a);
		if (r < 0)
			return n;
		if (r == 0) {
			avdec_seek(a, 0);
			a->clock = 0;
			break;
		}
		n++;
	}
	return n > 0;
}

double
avdec_time(const avdec_t *a)
{
	if (!a)
		return 0;
	return a->pts;
}

void
avdec_seek(avdec_t *a, double seconds)
{
	int64_t ts;
	if (!a || !a->fmt)
		return;
	if (seconds < 0)
		seconds = 0;
	ts = (int64_t)(seconds * (double)AV_TIME_BASE);
	av_seek_frame(a->fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
	if (a->codec)
		avcodec_flush_buffers(a->codec);
	if (a->acodec)
		avcodec_flush_buffers(a->acodec);
	if (a->hw)
		hwdec_reset(a->hw);
	a->sent_prefix = 0;
	a->eof = 0;
	a->clock = seconds;
	a->pts = seconds;
	pull_one(a);
}

const char *
avdec_mode(const avdec_t *a)
{
	if (a && a->hw)
		return "HW";
	return "SW";
}
