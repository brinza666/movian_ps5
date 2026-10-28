#include "avdec.h"

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
pull_one(avdec_t *a)
{
	int r, spins = 0;
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
		if (a->pkt->stream_index == a->vindex)
			avcodec_send_packet(a->codec, a->pkt);
		av_packet_unref(a->pkt);
	}
}

int
avdec_open(avdec_t **out, const char *path, uint8_t **rgba, int *w, int *h,
	   double *duration)
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
	a->pkt = av_packet_alloc();
	a->frame = av_frame_alloc();
	if (!a->rgba || !a->pkt || !a->frame)
		goto fail;
	memset(a->rgba, 0, (size_t)a->w * (size_t)a->h * 4);
	a->tb = st->time_base;
	a->pix = a->codec->pix_fmt;
	if (a->fmt->duration > 0)
		a->duration = (double)a->fmt->duration / (double)AV_TIME_BASE;
	else
		a->duration = 0;
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
	a->eof = 0;
	a->clock = seconds;
	a->pts = seconds;
	pull_one(a);
}
