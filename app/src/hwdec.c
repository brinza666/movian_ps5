#include "hwdec.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* Layout of libSceVideodec2, as used by the public players that call it
 * after sysmodule 207 (0xCF, libSceVideodec2) is loaded. Scanout is not
 * this device. Pictures are NV12 or P010 in the buffer we hand in.
 */

enum {
	VDEC_SYSMODULE = 207,
	VDEC_AVC = 1,
	VDEC_HEVC = 974921,
	VDEC_RESOURCE_COMPUTE = 1,
	/* VLC: 4 MiB slots, 8 in turn (3 broke a 4K keyframe); pictures
	 * stay held until display order (3 were overwritten: green).
	 * 1080p uses 16; 4K stays at 10 (memory). */
	VDEC_INPUT_BYTES = 4 << 20,
	VDEC_INPUT_SLOTS = 8,
	VDEC_FRAME_SLOTS = 16
};

typedef struct SceVideodec2DecoderConfigInfo {
	uint64_t size;
	uint32_t resource_type;
	uint32_t codec_type;
	uint32_t profile;
	uint32_t max_level;
	int32_t max_width;
	int32_t max_height;
	int32_t max_dpb_frames;
	uint32_t pipeline_depth;
	void *compute_queue;
	uint64_t cpu_affinity;
	int32_t cpu_priority;
	uint8_t optimize_progressive;
	uint8_t check_memory_type;
	uint8_t reserved0;
	uint8_t reserved1;
	void *extra_config;
} SceVideodec2DecoderConfigInfo;

typedef struct SceVideodec2DecoderMemoryInfo {
	uint64_t size;
	uint64_t cpu_size;
	void *cpu;
	uint64_t gpu_size;
	void *gpu;
	uint64_t cpu_gpu_size;
	void *cpu_gpu;
	uint64_t max_frame_size;
	uint32_t frame_alignment;
	uint32_t reserved;
} SceVideodec2DecoderMemoryInfo;

typedef struct SceVideodec2ComputeConfigInfo {
	uint64_t size;
	uint16_t pipe_id;
	uint16_t queue_id;
	uint8_t check_memory_type;
	uint8_t reserved0;
	uint16_t reserved1;
} SceVideodec2ComputeConfigInfo;

typedef struct SceVideodec2ComputeMemoryInfo {
	uint64_t size;
	uint64_t cpu_gpu_size;
	void *cpu_gpu;
} SceVideodec2ComputeMemoryInfo;

typedef struct SceVideodec2InputData {
	uint64_t size;
	void *au;
	uint64_t au_size;
	uint64_t pts;
	uint64_t dts;
	uint64_t attached;
} SceVideodec2InputData;

typedef struct SceVideodec2FrameBuffer {
	uint64_t size;
	void *buffer;
	uint64_t buffer_size;
	uint8_t accepted;
	uint8_t pad[7];
} SceVideodec2FrameBuffer;

typedef struct SceVideodec2OutputInfo {
	uint64_t size;
	uint8_t valid;
	uint8_t error;
	uint8_t picture_count;
	uint8_t padding;
	uint32_t codec;
	uint32_t width;
	uint32_t pitch;
	uint32_t height;
	uint32_t reserved;
	void *buffer;
	uint64_t buffer_size;
	uint32_t frame_format;
	uint32_t pitch_bytes;
} SceVideodec2OutputInfo;

typedef int (*vdec_query_compute_fn)(SceVideodec2ComputeMemoryInfo *);
typedef int (*vdec_alloc_queue_fn)(const SceVideodec2ComputeConfigInfo *,
				   const SceVideodec2ComputeMemoryInfo *, void **);
typedef int (*vdec_release_queue_fn)(void *);
typedef int (*vdec_query_decoder_fn)(const SceVideodec2DecoderConfigInfo *,
				     SceVideodec2DecoderMemoryInfo *);
typedef int (*vdec_create_fn)(const SceVideodec2DecoderConfigInfo *,
			      const SceVideodec2DecoderMemoryInfo *, void **);
typedef int (*vdec_delete_fn)(void *);
typedef int (*vdec_reset_fn)(void *);
typedef int (*vdec_decode_fn)(void *, SceVideodec2InputData *, SceVideodec2FrameBuffer *,
			      SceVideodec2OutputInfo *);
typedef int (*vdec_flush_fn)(void *, SceVideodec2FrameBuffer *, SceVideodec2OutputInfo *);
/* VLC: AVC picture info is 0x78, HEVC 0xB8. Head is size + valid + pts. */
typedef int (*vdec_picinfo_fn)(const SceVideodec2OutputInfo *, void *, void *);

typedef int (*k_alloc_fn)(int64_t, int64_t, size_t, size_t, int, int64_t *);
typedef int64_t (*k_dmsize_fn)(void);
typedef int (*k_map_fn)(void **, size_t, int, int, int64_t, size_t);
typedef int (*k_release_fn)(int64_t, size_t);
typedef int (*k_munmap_fn)(void *, size_t);
typedef int (*k_flex_fn)(void **, size_t, int, int, const char *);
typedef int (*k_flex_plain_fn)(void **, size_t, int, int);
extern int sceSysmoduleLoadModule(uint32_t module_id);

static vdec_query_compute_fn p_query_compute;
static vdec_alloc_queue_fn p_alloc_queue;
static vdec_release_queue_fn p_release_queue;
static vdec_query_decoder_fn p_query_decoder;
static vdec_create_fn p_create;
static vdec_delete_fn p_delete;
static vdec_reset_fn p_reset;
static vdec_decode_fn p_decode;
static vdec_flush_fn p_flush;
static vdec_picinfo_fn p_picinfo;

static k_alloc_fn p_alloc_dm;
static k_dmsize_fn p_dm_size;
static k_map_fn p_map_dm;
static k_release_fn p_release_dm;
static k_munmap_fn p_munmap;
static k_flex_fn p_flex;
static k_flex_plain_fn p_flex_plain;

static int g_probed;
static int g_ready;
static int g_force_sw;
static char g_status[24] = "SW";

/* Keep one compute queue for the title's lifetime. */
static void *g_queue;
static void *g_queue_mem;
static int64_t g_queue_pa = -1;
static size_t g_queue_bytes;

/* VLC: flexible memory given back never returned; one CPU block is reused. */
static void *g_cpu_block;
static size_t g_cpu_block_len;
static int g_cpu_block_busy;

struct hwdec {
	void *queue;
	void *decoder;
	void *compute_mem;
	int64_t compute_pa;
	size_t compute_bytes;
	void *cpu_mem;
	int64_t cpu_pa;
	size_t cpu_bytes;
	int cpu_direct;
	void *gpu_mem;
	int64_t gpu_pa;
	size_t gpu_bytes;
	void *cpu_gpu_mem;
	int64_t cpu_gpu_pa;
	size_t cpu_gpu_bytes;
	void *input_slot[VDEC_INPUT_SLOTS];
	int64_t input_slot_pa[VDEC_INPUT_SLOTS];
	size_t input_slot_len[VDEC_INPUT_SLOTS];
	void *frame_mem;
	int64_t frame_pa;
	size_t frame_bytes;
	size_t frame_size;
	uint8_t *rgba;
	int w, h;
	int rgba_w, rgba_h;
	int hevc;
	unsigned ring;
	unsigned errors;
	unsigned pictures;
	int dead;
	uint8_t *headers;
	int headers_n;
	int send_headers;
	int wait_key;
	int skip_rasl;
	int cpu_block;
	uint8_t frame_held[VDEC_FRAME_SLOTS];
	unsigned frame_count;
	unsigned diag_in;
	unsigned diag_out;
	/* VLC: pictures come out in display order; each takes the earliest
	 * packet time still waiting. Leading pictures before the restart
	 * keyframe are dropped (open GOP). */
	int64_t pending_pts[64];
	unsigned pending_n;
	int64_t key_pts;
	int64_t shown_pts;
	unsigned leading;
};

typedef char hwdec_cfg_sz[(sizeof(SceVideodec2DecoderConfigInfo) == 72) ? 1 : -1];
typedef char hwdec_out_sz[(sizeof(SceVideodec2OutputInfo) == 56) ? 1 : -1];
typedef char hwdec_fb_sz[(sizeof(SceVideodec2FrameBuffer) == 32) ? 1 : -1];

static size_t
align16k(size_t v)
{
	return (v + 0x3fffu) & ~(size_t)0x3fff;
}

static int alloc_dm(size_t size, int prot, int64_t *pa, void **va);
static void free_dm(void *va, int64_t pa, size_t size);

/* The title loader binds these imports to the console libraries. */
extern int sceVideodec2QueryComputeMemoryInfo(SceVideodec2ComputeMemoryInfo *);
extern int sceVideodec2AllocateComputeQueue(const SceVideodec2ComputeConfigInfo *,
                                           const SceVideodec2ComputeMemoryInfo *, void **);
extern int sceVideodec2ReleaseComputeQueue(void *);
extern int sceVideodec2QueryDecoderMemoryInfo(const SceVideodec2DecoderConfigInfo *,
                                              SceVideodec2DecoderMemoryInfo *);
extern int sceVideodec2CreateDecoder(const SceVideodec2DecoderConfigInfo *,
                                     const SceVideodec2DecoderMemoryInfo *, void **);
extern int sceVideodec2DeleteDecoder(void *);
extern int sceVideodec2Reset(void *);
extern int sceVideodec2Decode(void *, SceVideodec2InputData *, SceVideodec2FrameBuffer *,
                            SceVideodec2OutputInfo *);
extern int sceVideodec2Flush(void *, SceVideodec2FrameBuffer *, SceVideodec2OutputInfo *);
extern int sceVideodec2GetPictureInfo(const SceVideodec2OutputInfo *, void *, void *);

extern int sceKernelAllocateDirectMemory(int64_t, int64_t, size_t, size_t, int, int64_t *);
extern int64_t sceKernelGetDirectMemorySize(void);
extern int sceKernelMapDirectMemory(void **, size_t, int, int, int64_t, size_t);
extern int sceKernelReleaseDirectMemory(int64_t, size_t);
extern int sceKernelMunmap(void *, size_t);
extern int sceKernelMapNamedFlexibleMemory(void **, size_t, int, int, const char *);
extern int sceKernelMapFlexibleMemory(void **, size_t, int, int);

static void
bind_decoder_imports(void)
{
    p_query_compute = sceVideodec2QueryComputeMemoryInfo;
    p_alloc_queue = sceVideodec2AllocateComputeQueue;
    p_release_queue = sceVideodec2ReleaseComputeQueue;
    p_query_decoder = sceVideodec2QueryDecoderMemoryInfo;
    p_create = sceVideodec2CreateDecoder;
    p_delete = sceVideodec2DeleteDecoder;
    p_reset = sceVideodec2Reset;
    p_decode = sceVideodec2Decode;
    p_flush = sceVideodec2Flush;
    p_picinfo = sceVideodec2GetPictureInfo;
    p_alloc_dm = sceKernelAllocateDirectMemory;
    p_dm_size = sceKernelGetDirectMemorySize;
    p_map_dm = sceKernelMapDirectMemory;
    p_release_dm = sceKernelReleaseDirectMemory;
    p_munmap = sceKernelMunmap;
    p_flex = sceKernelMapNamedFlexibleMemory;
    p_flex_plain = sceKernelMapFlexibleMemory;
}

static void
record_probe(const char *stage, const char *detail)
{
    FILE *log = fopen("/download0/hevc-test-probe.log", "a");
    if (!log)
        return;
    fprintf(log, "%s: %s\n", stage, detail ? detail : "no detail");
    fclose(log);
}

static void
record_decoder_result(const char *stage, const SceVideodec2DecoderConfigInfo *cfg, int rc)
{
    char detail[96];
    snprintf(detail, sizeof detail, "profile=%u level=%u depth=%u rc=%#x",
             cfg->profile, cfg->max_level, cfg->pipeline_depth, (unsigned)rc);
    record_probe(stage, detail);
}

static void
record_memory_result(const char *stage, size_t bytes, int rc)
{
    char detail[96];
    snprintf(detail, sizeof detail, "bytes=%zu rc=%#x", bytes, (unsigned)rc);
    record_probe(stage, detail);
}

static bool
load_videodec_module(void)
{
    const int rc = sceSysmoduleLoadModule(VDEC_SYSMODULE);
    char result[32];
    snprintf(result, sizeof result, "%#x", (unsigned)rc);
    record_probe("Standard sceSysmoduleLoadModule", result);

    if (rc != 0)
        return false;
    bind_decoder_imports();
    record_probe("Videodec2 exports", "using title imports");
    return true;
}

static void
fill_config(SceVideodec2DecoderConfigInfo *cfg, uint32_t codec, uint32_t profile, uint32_t level,
	    uint32_t depth, int32_t w, int32_t h)
{
	memset(cfg, 0, sizeof *cfg);
	cfg->size = sizeof *cfg;
	cfg->resource_type = VDEC_RESOURCE_COMPUTE;
	cfg->codec_type = codec;
	cfg->profile = profile;
	cfg->max_level = level;
	cfg->max_width = w;
	cfg->max_height = h;
	cfg->max_dpb_frames = 16;
	cfg->pipeline_depth = depth;
	cfg->compute_queue = g_queue;
	cfg->cpu_affinity = 0x3f;
	cfg->cpu_priority = 700;
	cfg->optimize_progressive = 1;
}

static int __attribute__((unused))
make_compute_queue(void)
{
	SceVideodec2ComputeMemoryInfo cm;
	SceVideodec2ComputeConfigInfo cc;
	int rc;
	if (g_queue)
		return 0;
	memset(&cm, 0, sizeof cm);
	cm.size = sizeof cm;
	rc = p_query_compute(&cm);
	if (rc)
		return rc;
	g_queue_bytes = align16k((size_t)cm.cpu_gpu_size);
	if (alloc_dm(g_queue_bytes, 0x33, &g_queue_pa, &g_queue_mem))
		return -1;
	cm.cpu_gpu = g_queue_mem;
	cm.cpu_gpu_size = g_queue_bytes;
	memset(&cc, 0, sizeof cc);
	cc.size = sizeof cc;
	rc = p_alloc_queue(&cc, &cm, &g_queue);
	if (rc || !g_queue) {
		free_dm(g_queue_mem, g_queue_pa, g_queue_bytes);
		g_queue = NULL;
		g_queue_mem = NULL;
		g_queue_pa = -1;
		g_queue_bytes = 0;
		fprintf(stderr, "[movian] videodec2 compute queue %#x\n", (unsigned)rc);
		return rc ? rc : -1;
	}
	fprintf(stderr, "[movian] videodec2 compute queue ready (%zu KiB)\n",
		g_queue_bytes >> 10);
	return 0;
}

int
hwdec_probe(void)
{
    if (g_probed)
        return g_ready ? 0 : -1;
    g_probed = 1;
    record_probe("Video test v14", "probe started");
    if (!load_videodec_module())
        return -1;
    const int queue_status = make_compute_queue();
    if (queue_status != 0) {
        char result[32];
        snprintf(result, sizeof result, "%#x", (unsigned)queue_status);
        record_probe("Videodec2 queue", result);
        return -1;
    }
    record_probe("Videodec2 queue", "ready");
    g_ready = 1;
    memcpy(g_status, "HW", 3);
    return 0;
}

int
hwdec_ready(void)
{
	if (g_force_sw)
		return 0;
	if (!g_probed)
		hwdec_probe();
	return g_ready;
}

void
hwdec_force_software(int on)
{
	g_force_sw = on ? 1 : 0;
	if (g_force_sw)
		memcpy(g_status, "SW", 3);
	else if (g_ready)
		memcpy(g_status, "HW", 3);
}

const char *
hwdec_status(void)
{
	if (!g_probed)
		return "SW";
	return g_status;
}

static int
alloc_dm(size_t size, int prot, int64_t *pa, void **va)
{
	int rc;
	*pa = -1;
	*va = NULL;
	if (!size)
		return 0;
	size = align16k(size);
	rc = p_alloc_dm(0, p_dm_size(), size, 0x4000, 12, pa);
	if (rc) {
		record_memory_result("direct allocate failed", size, rc);
		return rc;
	}
	rc = p_map_dm(va, size, prot, 0, *pa, 0x4000);
	if (rc || !*va) {
		record_memory_result("direct map failed", size, rc);
		p_release_dm(*pa, size);
		*pa = -1;
		*va = NULL;
		return rc ? rc : -1;
	}
	return 0;
}

static void
free_dm(void *va, int64_t pa, size_t size)
{
	if (!size)
		return;
	size = align16k(size);
	if (va && p_munmap) {
		const int rc = p_munmap(va, size);
		if (rc)
			record_memory_result("direct unmap failed", size, rc);
	}
	if (pa >= 0 && p_release_dm) {
		const int rc = p_release_dm(pa, size);
		if (rc)
			record_memory_result("direct release failed", size, rc);
	}
}

static int
map_flex(void **addr, size_t size)
{
	*addr = NULL;
	if (p_flex_plain && p_flex_plain(addr, size, 0x03, 0) == 0 && *addr)
		return 0;
	if (p_flex && p_flex(addr, size, 0x03, 0, "movian") == 0 && *addr)
		return 0;
	return -1;
}

static int
cpu_block_take(hwdec_t *h, size_t size, void **addr)
{
	size = align16k(size);
	*addr = NULL;
	if (!g_cpu_block_busy) {
		if (size > g_cpu_block_len) {
			void *grown = NULL;
			size_t want = size > ((size_t)96 << 20) ? size : ((size_t)96 << 20);
			if (map_flex(&grown, want) == 0) {
				if (g_cpu_block)
					record_memory_result("old decoder CPU unmap", g_cpu_block_len,
						p_munmap(g_cpu_block, g_cpu_block_len));
				g_cpu_block = grown;
				g_cpu_block_len = want;
			}
		}
		if (g_cpu_block && size <= g_cpu_block_len) {
			g_cpu_block_busy = 1;
			h->cpu_block = 1;
			h->cpu_bytes = size;
			*addr = g_cpu_block;
			return 0;
		}
	}
	if (map_flex(addr, size) == 0) {
		h->cpu_bytes = size;
		return 0;
	}
	if (alloc_dm(size, 0x33, &h->cpu_pa, addr))
		return -1;
	h->cpu_direct = 1;
	h->cpu_bytes = size;
	return 0;
}

static void
release_all(hwdec_t *h)
{
	if (!h)
		return;
	if (h->decoder && p_delete) {
		record_probe("Videodec2 decoder delete", "begin");
		record_memory_result("Videodec2 decoder delete", 0, p_delete(h->decoder));
	}
	h->decoder = NULL;
	/* The compute queue is process-wide; never release it here. */
	h->queue = NULL;
	free_dm(h->frame_mem, h->frame_pa, h->frame_bytes);
	{
		int s;
		for (s = 0; s < VDEC_INPUT_SLOTS; s++) {
			free_dm(h->input_slot[s], h->input_slot_pa[s], h->input_slot_len[s]);
			h->input_slot[s] = NULL;
			h->input_slot_pa[s] = -1;
			h->input_slot_len[s] = 0;
		}
	}
	free_dm(h->cpu_gpu_mem, h->cpu_gpu_pa, h->cpu_gpu_bytes);
	free_dm(h->gpu_mem, h->gpu_pa, h->gpu_bytes);
	if (h->cpu_mem) {
		if (h->cpu_block)
			g_cpu_block_busy = 0;
		else if (h->cpu_direct)
			free_dm(h->cpu_mem, h->cpu_pa, h->cpu_bytes);
		else if (p_munmap)
			record_memory_result("decoder CPU unmap", h->cpu_bytes,
				p_munmap(h->cpu_mem, h->cpu_bytes));
	}
	h->frame_mem = h->cpu_gpu_mem = h->gpu_mem = NULL;
	h->cpu_mem = h->compute_mem = NULL;
	h->cpu_direct = 0;
	h->cpu_block = 0;
	h->cpu_pa = h->gpu_pa = h->cpu_gpu_pa = h->frame_pa = -1;
	h->cpu_bytes = h->gpu_bytes = h->cpu_gpu_bytes = 0;
	h->frame_bytes = h->frame_size = 0;
	free(h->headers);
	h->headers = NULL;
	h->headers_n = 0;
}

static int
bring_up(hwdec_t *h, SceVideodec2DecoderConfigInfo *cfg, SceVideodec2DecoderMemoryInfo *mem)
{
	int rc;
	int i;
	memset(mem, 0, sizeof *mem);
	mem->size = sizeof *mem;
	rc = p_query_decoder(cfg, mem);
	record_decoder_result("Videodec2 decoder query", cfg, rc);
	if (rc)
		return rc;
	h->cpu_bytes = align16k((size_t)mem->cpu_size);
	if (mem->cpu_size && cpu_block_take(h, (size_t)mem->cpu_size, &mem->cpu))
		return -1;
	h->cpu_mem = mem->cpu;
	h->gpu_bytes = align16k((size_t)mem->gpu_size);
	h->cpu_gpu_bytes = align16k((size_t)mem->cpu_gpu_size);
	h->frame_size = align16k((size_t)mem->max_frame_size);
	if (!h->frame_size)
		return -1;
	if (h->frame_count < 1 || h->frame_count > VDEC_FRAME_SLOTS)
		h->frame_count = 10;
	h->frame_bytes = h->frame_size * h->frame_count;
	if (h->gpu_bytes) {
		if (alloc_dm(h->gpu_bytes, 0x33, &h->gpu_pa, &h->gpu_mem))
			return -1;
		mem->gpu = h->gpu_mem;
		mem->gpu_size = h->gpu_bytes;
	}
	if (h->cpu_gpu_bytes) {
		if (alloc_dm(h->cpu_gpu_bytes, 0x33, &h->cpu_gpu_pa, &h->cpu_gpu_mem))
			return -1;
		mem->cpu_gpu = h->cpu_gpu_mem;
		mem->cpu_gpu_size = h->cpu_gpu_bytes;
	}
	{
		int s;
		for (s = 0; s < VDEC_INPUT_SLOTS; s++) {
			h->input_slot_pa[s] = -1;
			h->input_slot_len[s] = (size_t)VDEC_INPUT_BYTES;
			if (alloc_dm(h->input_slot_len[s], 0x33, &h->input_slot_pa[s],
				     &h->input_slot[s]))
				return -1;
		}
	}
	if (alloc_dm(h->frame_bytes, 0x33, &h->frame_pa, &h->frame_mem))
		return -1;
	if (h->cpu_mem)
		memset(h->cpu_mem, 0, h->cpu_bytes);
	if (h->gpu_mem)
		memset(h->gpu_mem, 0, h->gpu_bytes);
	if (h->cpu_gpu_mem)
		memset(h->cpu_gpu_mem, 0, h->cpu_gpu_bytes);
	if (cfg->codec_type == (uint32_t)VDEC_HEVC && cfg->profile == 2) {
		uint64_t *q = (uint64_t *)h->frame_mem;
		size_t n = h->frame_bytes / 8;
		for (i = 0; (size_t)i < n; i++)
			q[i] = 0x0200020002000200ull;
	} else
		memset(h->frame_mem, 0x80, h->frame_bytes);
	rc = p_create(cfg, mem, &h->decoder);
	record_decoder_result("Videodec2 decoder create", cfg, rc);
	if (rc || !h->decoder) {
		h->decoder = NULL;
		return rc ? rc : -1;
	}
	return 0;
}

int
hwdec_open(hwdec_t **out, int hevc, int tenbit, int width, int height)
{
	hwdec_t *h;
	SceVideodec2DecoderConfigInfo cfg;
	SceVideodec2DecoderMemoryInfo mem;
	int32_t max_w, max_h;
	uint32_t codec, profile, level, depth;
	int rc;
	*out = NULL;
	if (!hwdec_ready())
		return -1;
	if (!g_queue)
		return -1;
	if (width < 16 || height < 16 || width > 3840 || height > 2176)
		return -1;
	h = (hwdec_t *)calloc(1, sizeof *h);
	if (!h)
		return -1;
	h->compute_pa = h->cpu_pa = h->gpu_pa = h->cpu_gpu_pa = -1;
	h->frame_pa = -1;
	{
		int s;
		for (s = 0; s < VDEC_INPUT_SLOTS; s++)
			h->input_slot_pa[s] = -1;
	}
	h->hevc = hevc;
	h->w = width;
	h->h = height;
	h->rgba_w = width;
	h->rgba_h = height;
	h->queue = g_queue;
	h->wait_key = 1;
	h->skip_rasl = hevc ? 1 : 0;
	h->send_headers = 1;
	/* VLC: decoder DPB plus pictures waiting to be shown. 10 overwrote
	 * B-frames at 1080p; 4K stays at 10 because each buffer is huge. */
	h->frame_count = (width <= 1920 && height <= 1088) ? VDEC_FRAME_SLOTS : 10u;
	h->key_pts = -1;
	h->shown_pts = -1;

	codec = hevc ? (uint32_t)VDEC_HEVC : (uint32_t)VDEC_AVC;
	/* Match the stream profile and use Kodi's bounds for 1080p and UHD. */
	profile = hevc ? (tenbit ? 2u : 1u) : 100u;
	level = hevc ? (width > 1920 || height > 1088 ? 153u : 123u) : (width > 1920 || height > 1088 ? 52u : 51u);
	max_w = width > 1920 || height > 1088 ? 3840 : 1920;
	/* VLC caches HEVC 4K at 2160, AVC at 2176. */
	max_h = height > 1088 ? (hevc ? 2160 : 2176) : 1088;
	if (max_w < ((width + 15) & ~15))
		max_w = (width + 15) & ~15;
	if (max_h < ((height + 15) & ~15))
		max_h = (height + 15) & ~15;
	depth = 1u;
	fill_config(&cfg, codec, profile, level, depth, max_w, max_h);
	rc = bring_up(h, &cfg, &mem);
	if (rc) {
		release_all(h);
		free(h);
		return rc;
	}
	/* VLC OpenDecoder: no Reset on a fresh decoder. Reset here made the
	 * chip wait for a keyframe it had already been given. */
	fprintf(stderr,
		"[movian] vdec %s %dx%d pipeline %u level %u frames %u ready (cpu %llu / gpu %llu / cpu+gpu %llu KiB)\n",
		hevc ? "HEVC" : "H.264", width, height, (unsigned)cfg.pipeline_depth,
		(unsigned)cfg.max_level, h->frame_count,
		(unsigned long long)(mem.cpu_size >> 10), (unsigned long long)(mem.gpu_size >> 10),
		(unsigned long long)(mem.cpu_gpu_size >> 10));
	*out = h;
	return 0;
}

void
hwdec_headers(hwdec_t *h, const uint8_t *annexb, int n)
{
	if (!h)
		return;
	free(h->headers);
	h->headers = NULL;
	h->headers_n = 0;
	h->send_headers = 1;
	if (!annexb || n <= 0)
		return;
	h->headers = (uint8_t *)malloc((size_t)n);
	if (!h->headers)
		return;
	memcpy(h->headers, annexb, (size_t)n);
	h->headers_n = n;
}

void
hwdec_bind(hwdec_t *h, uint8_t *rgba, int width, int height)
{
	if (!h)
		return;
	h->rgba = rgba;
	if (width > 0)
		h->rgba_w = width;
	if (height > 0)
		h->rgba_h = height;
}

int
hwdec_shown_width(const hwdec_t *h)
{
	return h ? h->w : 0;
}

int
hwdec_shown_height(const hwdec_t *h)
{
	return h ? h->h : 0;
}

int64_t
hwdec_shown_pts(const hwdec_t *h)
{
	return h ? h->shown_pts : -1;
}

static void
pending_clear(hwdec_t *h)
{
	h->pending_n = 0;
}

static void
pending_add(hwdec_t *h, int64_t pts)
{
	if (h->pending_n == 64) {
		memmove(h->pending_pts, h->pending_pts + 1,
			63 * sizeof h->pending_pts[0]);
		h->pending_n--;
	}
	h->pending_pts[h->pending_n++] = pts;
}

static int64_t
pending_take(hwdec_t *h)
{
	unsigned i, best;
	int64_t pts;
	if (!h->pending_n)
		return -1;
	best = 0;
	for (i = 1; i < h->pending_n; i++) {
		if (h->pending_pts[i] < 0)
			continue;
		if (h->pending_pts[best] < 0 || h->pending_pts[i] < h->pending_pts[best])
			best = i;
	}
	pts = h->pending_pts[best];
	h->pending_pts[best] = h->pending_pts[--h->pending_n];
	return pts;
}

void
hwdec_close(hwdec_t *h)
{
	if (!h)
		return;
	release_all(h);
	free(h);
}

void
hwdec_reset(hwdec_t *h)
{
	if (!h || !h->decoder || !p_reset)
		return;
	p_reset(h->decoder);
	h->ring = 0;
	h->errors = 0;
	h->dead = 0;
	h->send_headers = 1;
	h->wait_key = 1;
	h->skip_rasl = h->hevc;
	memset(h->frame_held, 0, sizeof h->frame_held);
	pending_clear(h);
	h->key_pts = -1;
	h->shown_pts = -1;
}

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
put_rgb(uint8_t *p, int y, int u, int v)
{
	int c = y - 16;
	int d = u - 128;
	int e = v - 128;
	p[0] = (uint8_t)clip8((298 * c + 409 * e + 128) >> 8);
	p[1] = (uint8_t)clip8((298 * c - 100 * d - 208 * e + 128) >> 8);
	p[2] = (uint8_t)clip8((298 * c + 516 * d + 128) >> 8);
	p[3] = 255;
}

static void
frame_back(hwdec_t *h, const void *buffer)
{
	const uint8_t *base, *b;
	size_t i;
	if (!h || !buffer || !h->frame_mem || !h->frame_size)
		return;
	base = (const uint8_t *)h->frame_mem;
	b = (const uint8_t *)buffer;
	if (b < base)
		return;
	i = (size_t)(b - base) / h->frame_size;
	if (i < h->frame_count)
		h->frame_held[i] = 0;
}

static int
next_frame(hwdec_t *h, SceVideodec2FrameBuffer *fb)
{
	unsigned i;
	for (i = 0; i < h->frame_count; i++) {
		if (h->frame_held[i])
			continue;
		memset(fb, 0, sizeof *fb);
		fb->size = sizeof *fb;
		fb->buffer = (uint8_t *)h->frame_mem + (size_t)i * h->frame_size;
		fb->buffer_size = h->frame_size;
		return (int)i;
	}
	return -1;
}

static unsigned
frame_luma(const SceVideodec2OutputInfo *out)
{
	const uint8_t *p = (const uint8_t *)out->buffer;
	unsigned w = out->width, ht = out->height;
	int ten;
	uint64_t sum = 0;
	unsigned y, x;
	if (!p || !w || !ht || !out->pitch_bytes)
		return 0;
	ten = out->pitch_bytes >= 2u * out->pitch && out->pitch > 0;
	for (y = 0; y < 16; y++) {
		const uint8_t *row = p + (size_t)(ht * y / 16 + ht / 32) * out->pitch_bytes;
		for (x = 0; x < 16; x++) {
			unsigned px = w * x / 16 + w / 32;
			sum += ten ? ((const uint16_t *)row)[px] >> 2 : row[px];
		}
	}
	return (unsigned)(sum / 256);
}

/* VLC output(): only the picture's real size, not the decoder's padding
 * (1088 rows for 1080). Chroma still starts after frame_height. */
static void
shown_size(hwdec_t *h, const SceVideodec2OutputInfo *out, int *w, int *hh)
{
	int vw = h->w;
	int vh = h->h;
	if (!vw || (out->width && (uint32_t)vw > out->width))
		vw = (int)out->width;
	if (!vh || (out->height && (uint32_t)vh > out->height))
		vh = (int)out->height;
	if (vw <= 0)
		vw = h->rgba_w > 0 ? h->rgba_w : (int)out->width;
	if (vh <= 0)
		vh = h->rgba_h > 0 ? h->rgba_h : (int)out->height;
	if (h->rgba_w > 0 && vw > h->rgba_w)
		vw = h->rgba_w;
	if (h->rgba_h > 0 && vh > h->rgba_h)
		vh = h->rgba_h;
	*w = vw;
	*hh = vh;
}

static int
copy_picture(hwdec_t *h, const SceVideodec2OutputInfo *out)
{
	int ten;
	uint32_t pitch;
	uint32_t rows;
	int y, x, w, hh;
	const uint8_t *base;
	const uint8_t *uv;
	uint8_t *dst;
	if (out->error || !out->valid || !out->buffer || !h->rgba)
		return 0;
	/* VLC: 10-bit samples sit at the bottom of 16 bits, not P010.
	 * Chroma follows the decoder's luma rows (1088 for a padded 1080). */
	ten = out->pitch_bytes >= out->pitch * 2u && out->pitch > 0;
	pitch = out->pitch_bytes ? out->pitch_bytes : out->pitch;
	if (!pitch)
		return 0;
	if (ten && pitch < out->pitch * 2u)
		pitch = out->pitch * 2u;
	rows = out->height ? out->height : (uint32_t)h->h;
	shown_size(h, out, &w, &hh);
	if (w <= 0 || hh <= 0)
		return 0;
	base = (const uint8_t *)out->buffer;
	uv = base + (size_t)pitch * rows;
	dst = h->rgba;
	/* Pack at the shown size (VLC visible). Canvas uses w as the stride. */
	for (y = 0; y < hh; y++) {
		uint8_t *row = dst + (size_t)y * (size_t)w * 4u;
		for (x = 0; x < w; x++) {
			int Y, U, V;
			if (!ten) {
				const uint8_t *yp = base + (size_t)y * pitch;
				const uint8_t *up = uv + (size_t)(y / 2) * pitch;
				Y = yp[x];
				U = up[x & ~1];
				V = up[(x & ~1) + 1];
			} else {
				const uint16_t *yp = (const uint16_t *)(base + (size_t)y * pitch);
				const uint16_t *up = (const uint16_t *)(uv + (size_t)(y / 2) * pitch);
				Y = yp[x] >> 2;
				U = up[x & ~1] >> 2;
				V = up[(x & ~1) + 1] >> 2;
			}
			put_rgb(row + (size_t)x * 4u, Y, U, V);
		}
	}
	if (h->w != w || h->h != hh) {
		fprintf(stderr, "[movian] vdec pictures %ux%u (shown %dx%d) %s pitch %u\n",
			(unsigned)out->width, (unsigned)out->height, w, hh,
			ten ? "10-bit" : "NV12", (unsigned)pitch);
		h->w = w;
		h->h = hh;
	}
	return 1;
}

static int
start_code_len(const uint8_t *p, int n, int i)
{
	if (i + 3 > n)
		return 0;
	if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1)
		return 3;
	if (i + 4 <= n && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1)
		return 4;
	return 0;
}

/* VLC: drop Dolby Vision (62/63) and any layer other than the base. */
static int
hevc_skip_nal(const uint8_t *nal, int n)
{
	unsigned type, layer;
	if (n < 2)
		return 1;
	type = (nal[0] >> 1) & 0x3f;
	layer = ((nal[0] & 1u) << 5) | (nal[1] >> 3);
	return type == 62 || type == 63 || layer != 0;
}

static int
hevc_keep(const uint8_t *au, int size, uint8_t *dst, int room)
{
	int i = 0;
	int o = 0;
	while (i + 3 < size) {
		int sc, nal, next;
		sc = start_code_len(au, size, i);
		if (!sc) {
			i++;
			continue;
		}
		nal = i + sc;
		if (nal >= size)
			break;
		next = nal;
		while (next + 3 < size && !start_code_len(au, size, next))
			next++;
		if (next + 3 >= size)
			next = size;
		if (!hevc_skip_nal(au + nal, next - nal) && o + (next - i) <= room) {
			memcpy(dst + o, au + i, (size_t)(next - i));
			o += next - i;
		}
		i = next;
	}
	if (o <= 0) {
		if (size > room)
			return -1;
		memcpy(dst, au, (size_t)size);
		return size;
	}
	return o;
}

static int
is_keyframe(const uint8_t *p, int n, int hevc)
{
	int i;
	for (i = 0; i + 4 < n; i++) {
		int sc = start_code_len(p, n, i);
		const uint8_t *nal;
		if (!sc)
			continue;
		nal = p + i + sc;
		if (hevc) {
			unsigned type = (nal[0] >> 1) & 0x3f;
			if (type < 32)
				return type >= 16 && type <= 21;
		} else if ((nal[0] & 0x1f) == 5)
			return 1;
		i += sc - 1;
	}
	return 0;
}

static int
hevc_first_slice_type(const uint8_t *p, int n)
{
	int i;
	for (i = 0; i + 4 < n; i++) {
		int sc = start_code_len(p, n, i);
		unsigned type;
		if (!sc)
			continue;
		type = (p[i + sc] >> 1) & 0x3f;
		if (type < 32)
			return (int)type;
		i += sc - 1;
	}
	return -1;
}

static int
take_picture(hwdec_t *h, const SceVideodec2OutputInfo *out)
{
	int got;
	int64_t when = -1;
	if (!out->valid) {
		if (out->error)
			h->errors++;
		return 0;
	}
	when = pending_take(h);
	if (h->diag_out < 8) {
		long idx = -1;
		int prc = -1;
		unsigned long long chip = 0;
		h->diag_out++;
		if (h->frame_mem && out->buffer && h->frame_size)
			idx = (long)(((const uint8_t *)out->buffer - (const uint8_t *)h->frame_mem) /
				     (ptrdiff_t)h->frame_size);
		if (p_picinfo) {
			uint64_t info[32];
			uint64_t info2[32];
			memset(info, 0, sizeof info);
			memset(info2, 0, sizeof info2);
			info[0] = info2[0] = h->hevc ? 0xB8ull : 0x78ull;
			prc = p_picinfo(out, info, info2);
			if (prc == 0 && ((const uint8_t *)info)[8])
				chip = info[2];
		}
		fprintf(stderr,
			"[movian] vdec out %u: buf %ld our %lld chip %llu info %#x luma %u wait %u %ux%u\n",
			h->diag_out, idx, (long long)when, chip, (unsigned)prc, frame_luma(out),
			h->pending_n, (unsigned)out->width, (unsigned)out->height);
	}
	/* VLC: open-GOP leading pictures refer to frames the chip never got. */
	if (when >= 0 && h->key_pts >= 0 && when < h->key_pts) {
		if (h->leading++ == 0)
			fprintf(stderr, "[movian] vdec: pictures before the first keyframe left out\n");
		frame_back(h, out->buffer);
		return 0;
	}
	if (out->error) {
		h->errors++;
		frame_back(h, out->buffer);
		return 0;
	}
	got = copy_picture(h, out);
	frame_back(h, out->buffer);
	if (got) {
		h->pictures++;
		h->shown_pts = when;
	}
	return got;
}

int
hwdec_feed(hwdec_t *h, const uint8_t *au, int au_size, int64_t pts_us)
{
	unsigned slot_in;
	int slot_fr;
	uint8_t *in;
	int size = 0;
	int kept;
	SceVideodec2InputData input;
	SceVideodec2FrameBuffer frame;
	SceVideodec2OutputInfo out;
	int rc;
	if (!h || h->dead || !h->decoder || !au || au_size <= 0)
		return h && h->dead ? -1 : 0;
	slot_in = h->ring % VDEC_INPUT_SLOTS;
	slot_fr = next_frame(h, &frame);
	if (slot_fr < 0) {
		/* VLC: never reuse a held picture. 16 slots at 1080p is the
		 * fix; flushing here would paint over Movian's single canvas. */
		fprintf(stderr, "[movian] vdec: no free picture buffer (%u)\n", h->frame_count);
		return 0;
	}
	/* VLC: a 4K keyframe can pass 4 MiB; grow the slot instead of
	 * cutting it short (that was INVALID_SEQUENCE from then on). */
	{
		size_t need = (size_t)h->headers_n + (size_t)au_size + 64;
		if (need > h->input_slot_len[slot_in]) {
			free_dm(h->input_slot[slot_in], h->input_slot_pa[slot_in],
				h->input_slot_len[slot_in]);
			h->input_slot[slot_in] = NULL;
			h->input_slot_pa[slot_in] = -1;
			h->input_slot_len[slot_in] = align16k(need + ((size_t)1 << 20));
			if (alloc_dm(h->input_slot_len[slot_in], 0x33, &h->input_slot_pa[slot_in],
				     &h->input_slot[slot_in])) {
				fprintf(stderr, "[movian] vdec: no memory for AU %d\n", au_size);
				return -1;
			}
			fprintf(stderr, "[movian] vdec AU slot grown to %zu KiB\n",
				h->input_slot_len[slot_in] >> 10);
		}
	}
	in = (uint8_t *)h->input_slot[slot_in];
	if (h->send_headers && h->headers_n > 0 &&
	    (size_t)h->headers_n < h->input_slot_len[slot_in]) {
		memcpy(in, h->headers, (size_t)h->headers_n);
		size = h->headers_n;
	}
	if (h->hevc) {
		kept = hevc_keep(au, au_size, in + size,
				 (int)h->input_slot_len[slot_in] - size);
		if (kept <= 0)
			return 0;
		size += kept;
	} else {
		if ((size_t)size + (size_t)au_size > h->input_slot_len[slot_in])
			return 0;
		memcpy(in + size, au, (size_t)au_size);
		size += au_size;
	}
	if (size <= 0)
		return 0;
	/* VLC: after reset / open-GOP, only a keyframe (IRAP / IDR). */
	if (h->wait_key && !is_keyframe(in, size, h->hevc))
		return 0;
	if (h->wait_key) {
		h->wait_key = 0;
		h->skip_rasl = h->hevc;
		h->key_pts = pts_us;
	}
	if (h->skip_rasl && h->hevc) {
		int type = hevc_first_slice_type(in, size);
		if (type == 8 || type == 9)
			return 0;
		if (type >= 0 && type != 16 && type != 17 && type != 18 && type != 19 &&
		    type != 20 && type != 21)
			h->skip_rasl = 0;
	}
	h->ring++;
	memset(&input, 0, sizeof input);
	memset(&out, 0, sizeof out);
	input.size = sizeof input;
	input.au = in;
	input.au_size = (uint64_t)size;
	input.pts = pts_us >= 0 ? (uint64_t)pts_us : 0;
	input.dts = ~(uint64_t)0;
	out.size = sizeof out;
	rc = p_decode(h->decoder, &input, &frame, &out);
	if (frame.accepted)
		h->frame_held[slot_fr] = 1;
	if (h->diag_in < 8) {
		h->diag_in++;
		fprintf(stderr, "[movian] vdec in %u: %d bytes pts=%lld key=%d buf %d %s rc=%#x pic=%d\n",
			h->diag_in, size, (long long)pts_us, is_keyframe(in, size, h->hevc), slot_fr,
			frame.accepted ? "held" : "free", (unsigned)rc, (int)out.valid);
	}
	if (rc) {
		unsigned u = (unsigned)rc;
		if (u >= 0x811D0300u && u <= 0x811D0304u && p_reset) {
			p_reset(h->decoder);
			h->send_headers = 1;
			h->wait_key = 1;
			h->skip_rasl = h->hevc;
			memset(h->frame_held, 0, sizeof h->frame_held);
			pending_clear(h);
			h->key_pts = -1;
			return 0;
		}
		if (++h->errors >= 8 && h->pictures == 0) {
			h->dead = 1;
			return -1;
		}
		return 0;
	}
	h->send_headers = 0;
	h->errors = 0;
	/* VLC: this AU's time waits until a picture comes out in display order. */
	pending_add(h, pts_us);
	return take_picture(h, &out);
}

int
hwdec_flush(hwdec_t *h)
{
	int got = 0;
	int i;
	if (!h || !h->decoder || !p_flush)
		return 0;
	/* VLC drain(): pictures still inside after EOF. One shown picture
	 * per call — we have a single RGBA canvas, not VLC's picture queue. */
	for (i = 0; i < 32; i++) {
		SceVideodec2FrameBuffer frame;
		SceVideodec2OutputInfo out;
		int slot = next_frame(h, &frame);
		int rc;
		if (slot < 0)
			break;
		memset(&out, 0, sizeof out);
		out.size = sizeof out;
		rc = p_flush(h->decoder, &frame, &out);
		if (frame.accepted)
			h->frame_held[slot] = 1;
		if (rc || !out.valid)
			break;
		if (take_picture(h, &out) > 0)
			return 1;
	}
	return got;
}

static int
append_start(uint8_t *dst, int cap, int o, const uint8_t *nal, int nal_n)
{
	if (nal_n <= 0 || o < 0 || o + 4 + nal_n > cap)
		return -1;
	dst[o] = 0;
	dst[o + 1] = 0;
	dst[o + 2] = 0;
	dst[o + 3] = 1;
	memcpy(dst + o + 4, nal, (size_t)nal_n);
	return o + 4 + nal_n;
}

static int
looks_annexb(const uint8_t *p, int n)
{
	if (n >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1)
		return 1;
	if (n >= 3 && p[0] == 0 && p[1] == 0 && p[2] == 1)
		return 1;
	return 0;
}

int
hwdec_extradata_annexb(int hevc, const uint8_t *extra, int extra_n,
		       uint8_t *dst, int dst_cap, int *nal_size)
{
	int o = 0;
	*nal_size = 0;
	if (!extra || extra_n <= 0)
		return 0;
	if (looks_annexb(extra, extra_n)) {
		if (extra_n > dst_cap)
			return -1;
		memcpy(dst, extra, (size_t)extra_n);
		return extra_n;
	}
	if (!hevc && extra[0] == 1 && extra_n >= 7) {
		int nal_len = (extra[4] & 3) + 1;
		int num_sps = extra[5] & 0x1f;
		int pos = 6;
		int i;
		*nal_size = nal_len;
		for (i = 0; i < num_sps; i++) {
			int len;
			if (pos + 2 > extra_n)
				return -1;
			len = (extra[pos] << 8) | extra[pos + 1];
			pos += 2;
			if (pos + len > extra_n)
				return -1;
			o = append_start(dst, dst_cap, o, extra + pos, len);
			if (o < 0)
				return -1;
			pos += len;
		}
		if (pos >= extra_n)
			return o;
		{
			int num_pps = extra[pos++];
			for (i = 0; i < num_pps; i++) {
				int len;
				if (pos + 2 > extra_n)
					return -1;
				len = (extra[pos] << 8) | extra[pos + 1];
				pos += 2;
				if (pos + len > extra_n)
					return -1;
				o = append_start(dst, dst_cap, o, extra + pos, len);
				if (o < 0)
					return -1;
				pos += len;
			}
		}
		return o;
	}
	if (hevc && extra[0] == 1 && extra_n >= 23) {
		int num_arrays = extra[22];
		int pos = 23;
		int a;
		*nal_size = (extra[21] & 3) + 1;
		for (a = 0; a < num_arrays; a++) {
			int count, k;
			if (pos + 3 > extra_n)
				return -1;
			count = (extra[pos + 1] << 8) | extra[pos + 2];
			pos += 3;
			for (k = 0; k < count; k++) {
				int len;
				if (pos + 2 > extra_n)
					return -1;
				len = (extra[pos] << 8) | extra[pos + 1];
				pos += 2;
				if (pos + len > extra_n)
					return -1;
				o = append_start(dst, dst_cap, o, extra + pos, len);
				if (o < 0)
					return -1;
				pos += len;
			}
		}
		return o;
	}
	return 0;
}

int
hwdec_packet_annexb(const uint8_t *src, int src_n, int nal_size,
		    uint8_t *dst, int dst_cap)
{
	int i, o;
	if (!src || src_n <= 0)
		return -1;
	if (looks_annexb(src, src_n) || nal_size <= 0) {
		if (src_n > dst_cap)
			return -1;
		memcpy(dst, src, (size_t)src_n);
		return src_n;
	}
	if (nal_size < 1 || nal_size > 4)
		return -1;
	i = 0;
	o = 0;
	while (i + nal_size <= src_n) {
		int len = 0;
		int k;
		for (k = 0; k < nal_size; k++)
			len = (len << 8) | src[i++];
		if (len <= 0 || i + len > src_n)
			return -1;
		o = append_start(dst, dst_cap, o, src + i, len);
		if (o < 0)
			return -1;
		i += len;
	}
	return o;
}

typedef struct {
	const uint8_t *p;
	size_t nbits;
	size_t bit;
} Bits;

static unsigned
bits_left(const Bits *b)
{
	return b->bit < b->nbits ? (unsigned)(b->nbits - b->bit) : 0;
}

static unsigned
get_bits(Bits *b, int n)
{
	unsigned v = 0;
	int i;
	for (i = 0; i < n; i++) {
		size_t t = b->bit;
		if (t >= b->nbits)
			return v;
		b->bit++;
		v = (v << 1) | ((unsigned)(b->p[t / 8] >> (7 - (t % 8))) & 1u);
	}
	return v;
}

static unsigned
get_ue(Bits *b)
{
	unsigned z = 0;
	while (bits_left(b) &&
	       !((b->p[b->bit / 8] >> (7 - (b->bit % 8))) & 1) && z < 31) {
		b->bit++;
		z++;
	}
	if (bits_left(b))
		b->bit++;
	return ((1u << z) - 1) + (z ? get_bits(b, (int)z) : 0);
}

static int
get_se(Bits *b)
{
	unsigned u = get_ue(b);
	if (u & 1)
		return (int)((u + 1) / 2);
	return -(int)(u / 2);
}

static int
nal_to_rbsp(const uint8_t *nal, int n, uint8_t *dst, int cap)
{
	int i = 0;
	int o = 0;
	while (i < n && o < cap) {
		if (i + 2 < n && nal[i] == 0 && nal[i + 1] == 0 && nal[i + 2] == 3) {
			if (o + 2 > cap)
				break;
			dst[o++] = 0;
			dst[o++] = 0;
			i += 3;
			continue;
		}
		dst[o++] = nal[i++];
	}
	return o;
}

static int
sane_size(int w, int h)
{
	return w >= 16 && h >= 16 && w <= 3840 && h <= 2176;
}

/* H.264 SPS: coded size minus frame_cropping (VLC visible, not 1088 padding). */
static int
parse_h264_sps(const uint8_t *rbsp, int n, int *ow, int *oh)
{
	Bits b;
	unsigned profile, chroma = 1, separate = 0;
	unsigned w_mbs, h_map, frame_mbs_only, crop;
	unsigned left = 0, right = 0, top = 0, bottom = 0;
	unsigned sub_w = 2, sub_h = 2;
	int width, height;
	unsigned poc, i, n_ref;
	if (n < 4)
		return -1;
	b.p = rbsp;
	b.nbits = (size_t)n * 8;
	b.bit = 0;
	profile = get_bits(&b, 8);
	get_bits(&b, 16); /* constraints + level */
	get_ue(&b);       /* sps id */
	if (profile == 100 || profile == 110 || profile == 122 || profile == 244 ||
	    profile == 44 || profile == 83 || profile == 86 || profile == 118 ||
	    profile == 128 || profile == 138 || profile == 139 || profile == 134 ||
	    profile == 135) {
		chroma = get_ue(&b);
		if (chroma == 3)
			separate = get_bits(&b, 1);
		get_ue(&b);
		get_ue(&b);
		get_bits(&b, 1);
		if (get_bits(&b, 1))
			return -1; /* scaling list: leave size to the container */
	}
	get_ue(&b); /* log2_max_frame_num */
	poc = get_ue(&b);
	if (poc == 0)
		get_ue(&b);
	else if (poc == 1) {
		get_bits(&b, 1);
		get_se(&b);
		get_se(&b);
		n_ref = get_ue(&b);
		for (i = 0; i < n_ref && i < 32; i++)
			get_se(&b);
	}
	get_ue(&b);
	get_bits(&b, 1);
	w_mbs = get_ue(&b);
	h_map = get_ue(&b);
	frame_mbs_only = get_bits(&b, 1);
	if (!frame_mbs_only)
		get_bits(&b, 1);
	get_bits(&b, 1);
	crop = get_bits(&b, 1);
	if (crop) {
		left = get_ue(&b);
		right = get_ue(&b);
		top = get_ue(&b);
		bottom = get_ue(&b);
	}
	width = (int)((w_mbs + 1) * 16);
	height = (int)((2 - frame_mbs_only) * (h_map + 1) * 16);
	if (chroma == 0 || separate) {
		sub_w = 1;
		sub_h = 1;
	} else if (chroma == 2)
		sub_h = 1;
	if (crop) {
		width -= (int)((left + right) * sub_w);
		height -= (int)((top + bottom) * sub_h * (2 - frame_mbs_only));
	}
	if (!sane_size(width, height))
		return -1;
	*ow = width;
	*oh = height;
	return 0;
}

static int
skip_hevc_ptl(Bits *b, unsigned max_sub)
{
	unsigned i;
	unsigned present[8];
	get_bits(b, 3);
	get_bits(b, 5);
	get_bits(b, 32);
	get_bits(b, 4);
	/* Main / Main 10: 44 reserved, then inbld, then level. */
	get_bits(b, 44);
	get_bits(b, 1);
	get_bits(b, 8);
	if (max_sub > 7)
		max_sub = 7;
	for (i = 0; i < max_sub; i++)
		present[i] = get_bits(b, 2);
	if (max_sub > 0) {
		for (i = max_sub; i < 8; i++)
			get_bits(b, 2);
	}
	for (i = 0; i < max_sub; i++) {
		if (present[i] & 2) {
			get_bits(b, 2 + 1 + 5 + 32 + 4 + 44);
			get_bits(b, 1);
		}
		if (present[i] & 1)
			get_bits(b, 8);
	}
	return bits_left(b) ? 0 : -1;
}

/* HEVC SPS: pic_* minus conformance_window (1088 -> 1080). */
static int
parse_hevc_sps(const uint8_t *rbsp, int n, int *ow, int *oh)
{
	Bits b;
	unsigned max_sub, chroma, w, h, win;
	if (n < 8)
		return -1;
	b.p = rbsp;
	b.nbits = (size_t)n * 8;
	b.bit = 0;
	get_bits(&b, 4);
	max_sub = get_bits(&b, 3);
	get_bits(&b, 1);
	if (skip_hevc_ptl(&b, max_sub) < 0)
		return -1;
	get_ue(&b);
	chroma = get_ue(&b);
	if (chroma == 3)
		get_bits(&b, 1);
	w = get_ue(&b);
	h = get_ue(&b);
	win = get_bits(&b, 1);
	if (win) {
		unsigned left = get_ue(&b), right = get_ue(&b);
		unsigned top = get_ue(&b), bottom = get_ue(&b);
		unsigned sub_w = 1, sub_h = 1;
		if (chroma == 1) {
			sub_w = 2;
			sub_h = 2;
		} else if (chroma == 2)
			sub_w = 2;
		if (w > (left + right) * sub_w)
			w -= (left + right) * sub_w;
		if (h > (top + bottom) * sub_h)
			h -= (top + bottom) * sub_h;
	}
	if (!sane_size((int)w, (int)h))
		return -1;
	*ow = (int)w;
	*oh = (int)h;
	return 0;
}

int
hwdec_visible_from_headers(int hevc, const uint8_t *annexb, int n, int *w, int *h)
{
	int i = 0;
	uint8_t rbsp[512];
	if (!annexb || n <= 0 || !w || !h)
		return -1;
	*w = *h = 0;
	while (i + 3 < n) {
		int sc, nal, next, rn;
		sc = start_code_len(annexb, n, i);
		if (!sc) {
			i++;
			continue;
		}
		nal = i + sc;
		if (nal >= n)
			break;
		next = nal;
		while (next + 3 < n && !start_code_len(annexb, n, next))
			next++;
		if (next + 3 >= n)
			next = n;
		if (!hevc && (annexb[nal] & 0x1f) == 7) {
			rn = nal_to_rbsp(annexb + nal + 1, next - (nal + 1), rbsp, (int)sizeof rbsp);
			if (parse_h264_sps(rbsp, rn, w, h) == 0)
				return 0;
		}
		if (hevc && ((annexb[nal] >> 1) & 0x3f) == 33 && next - nal > 2) {
			rn = nal_to_rbsp(annexb + nal + 2, next - (nal + 2), rbsp, (int)sizeof rbsp);
			if (parse_hevc_sps(rbsp, rn, w, h) == 0)
				return 0;
		}
		i = next;
	}
	return -1;
}
