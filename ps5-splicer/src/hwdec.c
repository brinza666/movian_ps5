#include "hwdec.h"

#include <dlfcn.h>
#include <stddef.h>
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
	VDEC_INPUT_BYTES = 6 << 20,
	VDEC_INPUT_SLOTS = 2,
	VDEC_FRAME_SLOTS = 2
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
	uint64_t compute_queue;
	uint64_t cpu_affinity;
	int32_t cpu_priority;
	uint32_t optimize_progressive;
	uint32_t check_memory_type;
	uint32_t reserved;
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
	uint32_t accepted;
	uint32_t reserved;
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

typedef int (*k_load_fn)(const char *, uint64_t, const void *, uint32_t, void *, int *);
typedef int (*k_dlsym_fn)(int, const char *, void **);
typedef int (*k_alloc_fn)(int64_t, int64_t, size_t, size_t, int, int64_t *);
typedef int64_t (*k_dmsize_fn)(void);
typedef int (*k_map_fn)(void **, size_t, int, int, int64_t, size_t);
typedef int (*k_release_fn)(int64_t, size_t);
typedef int (*k_munmap_fn)(void *, size_t);
typedef int (*k_flex_fn)(void **, size_t, int, int, const char *);
typedef int (*k_unflex_fn)(void *, size_t);
typedef int (*sysmod_fn)(uint16_t);
typedef int (*sysmod_internal_fn)(uint32_t);

static vdec_query_compute_fn p_query_compute;
static vdec_alloc_queue_fn p_alloc_queue;
static vdec_release_queue_fn p_release_queue;
static vdec_query_decoder_fn p_query_decoder;
static vdec_create_fn p_create;
static vdec_delete_fn p_delete;
static vdec_reset_fn p_reset;
static vdec_decode_fn p_decode;
static vdec_flush_fn p_flush;

static k_load_fn p_load;
static k_dlsym_fn p_kdlsym;
static k_alloc_fn p_alloc_dm;
static k_dmsize_fn p_dm_size;
static k_map_fn p_map_dm;
static k_release_fn p_release_dm;
static k_munmap_fn p_munmap;
static k_flex_fn p_flex;
static k_unflex_fn p_unflex;

static int g_probed;
static int g_ready;
static char g_status[24] = "SW";

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
	void *input_mem;
	int64_t input_pa;
	size_t input_bytes;
	void *frame_mem;
	int64_t frame_pa;
	size_t frame_bytes;
	size_t frame_size;
	uint8_t *rgba;
	int w, h;
	int hevc;
	unsigned ring;
	unsigned errors;
	unsigned pictures;
	int dead;
};

typedef char hwdec_cfg_sz[(sizeof(SceVideodec2DecoderConfigInfo) == 72) ? 1 : -1];
typedef char hwdec_out_sz[(sizeof(SceVideodec2OutputInfo) == 56) ? 1 : -1];

static size_t
align16k(size_t v)
{
	return (v + 0x3fffu) & ~(size_t)0x3fff;
}

static void *
sym(void *dl, int handle, const char *name)
{
	void *p = NULL;
	if (dl)
		p = dlsym(dl, name);
	if (!p && handle > 0 && p_kdlsym)
		p_kdlsym(handle, name, &p);
	if (!p)
		p = dlsym(RTLD_DEFAULT, name);
	return p;
}

static int
load_module(const char *path)
{
	int res = 0;
	int handle;
	if (!p_load || !path)
		return -1;
	handle = p_load(path, 0, NULL, 0, NULL, &res);
	if (handle > 0)
		return handle;
	return -1;
}

static int
open_named(const char *const *paths, void **dl_out, int *handle_out)
{
	int i;
	*dl_out = NULL;
	*handle_out = 0;
	for (i = 0; paths[i]; i++) {
		if (paths[i][0] == '/')
			continue;
		*dl_out = dlopen(paths[i], RTLD_LAZY);
		if (*dl_out)
			return 0;
	}
	for (i = 0; paths[i]; i++) {
		int h = load_module(paths[i]);
		if (h > 0) {
			*handle_out = h;
			return 0;
		}
	}
	return -1;
}

static int
resolve_kernel(void)
{
	if (!p_load)
		p_load = (k_load_fn)dlsym(RTLD_DEFAULT, "sceKernelLoadStartModule");
	if (!p_kdlsym)
		p_kdlsym = (k_dlsym_fn)dlsym(RTLD_DEFAULT, "sceKernelDlsym");
	if (!p_alloc_dm)
		p_alloc_dm = (k_alloc_fn)dlsym(RTLD_DEFAULT, "sceKernelAllocateDirectMemory");
	if (!p_dm_size)
		p_dm_size = (k_dmsize_fn)dlsym(RTLD_DEFAULT, "sceKernelGetDirectMemorySize");
	if (!p_map_dm)
		p_map_dm = (k_map_fn)dlsym(RTLD_DEFAULT, "sceKernelMapDirectMemory");
	if (!p_release_dm)
		p_release_dm = (k_release_fn)dlsym(RTLD_DEFAULT, "sceKernelReleaseDirectMemory");
	if (!p_munmap)
		p_munmap = (k_munmap_fn)dlsym(RTLD_DEFAULT, "sceKernelMunmap");
	if (!p_flex)
		p_flex = (k_flex_fn)dlsym(RTLD_DEFAULT, "sceKernelMapNamedFlexibleMemory");
	if (!p_unflex)
		p_unflex = (k_unflex_fn)dlsym(RTLD_DEFAULT, "sceKernelReleaseFlexibleMemory");
	return p_load && p_alloc_dm && p_dm_size && p_map_dm && p_release_dm && p_munmap;
}

static void
load_videodec_module(void)
{
	static const char *sys_paths[] = {
		"libSceSysmodule.sprx",
		"/system/common/lib/libSceSysmodule.sprx",
		NULL
	};
	static const char *vdec_paths[] = {
		"libSceVideodec2.sprx",
		"/system/common/lib/libSceVideodec2.sprx",
		"/system/priv/lib/libSceVideodec2.sprx",
		"/user/homebrew/lib/libSceVideodec2.sprx",
		NULL
	};
	void *sys = NULL;
	void *vdec = NULL;
	int sys_h = 0;
	int vdec_h = 0;
	sysmod_fn load_mod;
	sysmod_internal_fn load_int;

	open_named(sys_paths, &sys, &sys_h);
	load_mod = (sysmod_fn)sym(sys, sys_h, "sceSysmoduleLoadModule");
	load_int = (sysmod_internal_fn)sym(sys, sys_h, "sceSysmoduleLoadModuleInternal");
	if (load_mod)
		load_mod(VDEC_SYSMODULE);
	if (load_int)
		load_int(VDEC_SYSMODULE);
	open_named(vdec_paths, &vdec, &vdec_h);

	p_query_compute = (vdec_query_compute_fn)sym(vdec, vdec_h, "sceVideodec2QueryComputeMemoryInfo");
	p_alloc_queue = (vdec_alloc_queue_fn)sym(vdec, vdec_h, "sceVideodec2AllocateComputeQueue");
	p_release_queue = (vdec_release_queue_fn)sym(vdec, vdec_h, "sceVideodec2ReleaseComputeQueue");
	p_query_decoder = (vdec_query_decoder_fn)sym(vdec, vdec_h, "sceVideodec2QueryDecoderMemoryInfo");
	p_create = (vdec_create_fn)sym(vdec, vdec_h, "sceVideodec2CreateDecoder");
	p_delete = (vdec_delete_fn)sym(vdec, vdec_h, "sceVideodec2DeleteDecoder");
	p_reset = (vdec_reset_fn)sym(vdec, vdec_h, "sceVideodec2Reset");
	p_decode = (vdec_decode_fn)sym(vdec, vdec_h, "sceVideodec2Decode");
	p_flush = (vdec_flush_fn)sym(vdec, vdec_h, "sceVideodec2Flush");
}

int
hwdec_probe(void)
{
	if (g_probed)
		return g_ready ? 0 : -1;
	g_probed = 1;
	g_ready = 0;
	memcpy(g_status, "SW", 3);
	if (!resolve_kernel())
		return -1;
	load_videodec_module();
	if (!p_query_compute || !p_alloc_queue || !p_release_queue || !p_query_decoder ||
	    !p_create || !p_delete || !p_reset || !p_decode)
		return -1;
	g_ready = 1;
	memcpy(g_status, "HW", 3);
	return 0;
}

int
hwdec_ready(void)
{
	if (!g_probed)
		hwdec_probe();
	return g_ready;
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
	if (rc)
		return rc;
	rc = p_map_dm(va, size, prot, 0, *pa, 0x4000);
	if (rc || !*va) {
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
	if (va && p_munmap)
		p_munmap(va, size);
	if (pa >= 0 && p_release_dm)
		p_release_dm(pa, size);
}

static void
release_all(hwdec_t *h)
{
	if (!h)
		return;
	if (h->decoder && p_delete)
		p_delete(h->decoder);
	h->decoder = NULL;
	if (h->queue && p_release_queue)
		p_release_queue(h->queue);
	h->queue = NULL;
	free_dm(h->frame_mem, h->frame_pa, h->frame_bytes);
	free_dm(h->input_mem, h->input_pa, h->input_bytes);
	free_dm(h->cpu_gpu_mem, h->cpu_gpu_pa, h->cpu_gpu_bytes);
	free_dm(h->gpu_mem, h->gpu_pa, h->gpu_bytes);
	if (h->cpu_mem) {
		if (h->cpu_direct)
			free_dm(h->cpu_mem, h->cpu_pa, h->cpu_bytes);
		else {
			if (p_unflex)
				p_unflex(h->cpu_mem, h->cpu_bytes);
			if (p_munmap)
				p_munmap(h->cpu_mem, h->cpu_bytes);
		}
	}
	free_dm(h->compute_mem, h->compute_pa, h->compute_bytes);
	h->frame_mem = h->input_mem = h->cpu_gpu_mem = h->gpu_mem = NULL;
	h->cpu_mem = h->compute_mem = NULL;
}

int
hwdec_open(hwdec_t **out, int hevc, int tenbit, int width, int height)
{
	hwdec_t *h;
	SceVideodec2ComputeMemoryInfo cm;
	SceVideodec2ComputeConfigInfo cc;
	SceVideodec2DecoderConfigInfo cfg;
	SceVideodec2DecoderMemoryInfo mem;
	int uhd;
	int rc;
	*out = NULL;
	if (!hwdec_ready())
		return -1;
	if (width < 16 || height < 16 || width > 3840 || height > 2176)
		return -1;
	if ((width | height) & 1)
		return -1;
	h = (hwdec_t *)calloc(1, sizeof *h);
	if (!h)
		return -1;
	h->compute_pa = h->cpu_pa = h->gpu_pa = h->cpu_gpu_pa = -1;
	h->input_pa = h->frame_pa = -1;
	h->hevc = hevc;
	h->w = width;
	h->h = height;

	memset(&cm, 0, sizeof cm);
	cm.size = sizeof cm;
	rc = p_query_compute(&cm);
	if (rc)
		goto fail;
	h->compute_bytes = align16k((size_t)cm.cpu_gpu_size);
	if (alloc_dm(h->compute_bytes, 0x33, &h->compute_pa, &h->compute_mem))
		goto fail;
	cm.cpu_gpu = h->compute_mem;
	cm.cpu_gpu_size = h->compute_bytes;
	memset(&cc, 0, sizeof cc);
	cc.size = sizeof cc;
	rc = p_alloc_queue(&cc, &cm, &h->queue);
	if (rc || !h->queue)
		goto fail;

	uhd = width > 1920 || height > 1088;
	memset(&cfg, 0, sizeof cfg);
	cfg.size = sizeof cfg;
	cfg.resource_type = VDEC_RESOURCE_COMPUTE;
	cfg.codec_type = hevc ? (uint32_t)VDEC_HEVC : (uint32_t)VDEC_AVC;
	cfg.profile = hevc ? (tenbit ? 2u : 1u) : 100u;
	cfg.max_level = hevc ? (uhd ? 153u : 123u) : (uhd ? 52u : 51u);
	cfg.max_width = uhd ? 3840 : 1920;
	cfg.max_height = uhd ? 2176 : 1088;
	cfg.max_dpb_frames = -1;
	cfg.pipeline_depth = 1;
	cfg.compute_queue = (uint64_t)(uintptr_t)h->queue;
	cfg.cpu_affinity = 0x3f;
	cfg.cpu_priority = 700;
	cfg.optimize_progressive = 1;
	memset(&mem, 0, sizeof mem);
	mem.size = sizeof mem;
	rc = p_query_decoder(&cfg, &mem);
	if (rc)
		goto fail;

	h->cpu_bytes = align16k((size_t)mem.cpu_size);
	if (p_flex)
		rc = p_flex(&mem.cpu, h->cpu_bytes, 0x03, 0, "splicer");
	else
		rc = -1;
	if (rc || !mem.cpu) {
		mem.cpu = NULL;
		if (alloc_dm(h->cpu_bytes, 0x33, &h->cpu_pa, &mem.cpu))
			goto fail;
		h->cpu_direct = 1;
	}
	h->cpu_mem = mem.cpu;

	h->gpu_bytes = align16k((size_t)mem.gpu_size);
	h->cpu_gpu_bytes = align16k((size_t)mem.cpu_gpu_size);
	h->frame_size = align16k((size_t)mem.max_frame_size);
	if (!h->frame_size)
		goto fail;
	h->input_bytes = (size_t)VDEC_INPUT_BYTES * VDEC_INPUT_SLOTS;
	h->frame_bytes = h->frame_size * VDEC_FRAME_SLOTS;
	mem.gpu_size = h->gpu_bytes;
	if (alloc_dm(h->gpu_bytes, 0x33, &h->gpu_pa, &h->gpu_mem))
		goto fail;
	mem.gpu = h->gpu_mem;
	if (h->cpu_gpu_bytes) {
		mem.cpu_gpu_size = h->cpu_gpu_bytes;
		if (alloc_dm(h->cpu_gpu_bytes, 0x33, &h->cpu_gpu_pa, &h->cpu_gpu_mem))
			goto fail;
		mem.cpu_gpu = h->cpu_gpu_mem;
	}
	if (alloc_dm(h->input_bytes, 0x33, &h->input_pa, &h->input_mem))
		goto fail;
	if (alloc_dm(h->frame_bytes, 0x33, &h->frame_pa, &h->frame_mem))
		goto fail;
	rc = p_create(&cfg, &mem, &h->decoder);
	if (rc || !h->decoder)
		goto fail;
	rc = p_reset(h->decoder);
	if (rc)
		goto fail;
	*out = h;
	return 0;
fail:
	release_all(h);
	free(h);
	return -1;
}

void
hwdec_bind(hwdec_t *h, uint8_t *rgba, int width, int height)
{
	if (!h)
		return;
	h->rgba = rgba;
	if (width > 0)
		h->w = width;
	if (height > 0)
		h->h = height;
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

static int
copy_picture(hwdec_t *h, const SceVideodec2OutputInfo *out)
{
	int ten;
	uint32_t pitch;
	uint32_t rows;
	uint32_t chroma_at;
	int y, x, w, hh;
	const uint8_t *base;
	const uint8_t *uv;
	uint8_t *dst;
	if (!out->valid || !out->buffer || !h->rgba)
		return 0;
	ten = out->pitch_bytes >= out->pitch * 2u && out->pitch > 0;
	pitch = out->pitch_bytes ? out->pitch_bytes : out->pitch;
	if (!pitch)
		return 0;
	if (ten && pitch < out->pitch * 2u)
		pitch = out->pitch * 2u;
	rows = out->height ? out->height : (uint32_t)h->h;
	chroma_at = rows;
	w = h->w;
	hh = h->h;
	if ((uint32_t)w > out->width && out->width)
		w = (int)out->width;
	if ((uint32_t)hh > rows)
		hh = (int)rows;
	base = (const uint8_t *)out->buffer;
	uv = base + (size_t)pitch * chroma_at;
	dst = h->rgba;
	for (y = 0; y < hh; y++) {
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
				Y = yp[x] >> 8;
				U = up[x & ~1] >> 8;
				V = up[(x & ~1) + 1] >> 8;
			}
			put_rgb(dst, Y, U, V);
			dst += 4;
		}
	}
	return 1;
}

static int
hevc_keep(const uint8_t *au, int size, uint8_t *dst)
{
	int i = 0;
	int o = 0;
	while (i + 3 < size) {
		int sc, nal, next, type;
		if (!(au[i] == 0 && au[i + 1] == 0 && (au[i + 2] == 1 || (au[i + 2] == 0 && au[i + 3] == 1)))) {
			i++;
			continue;
		}
		sc = i;
		nal = au[i + 2] == 1 ? i + 3 : i + 4;
		if (nal >= size)
			break;
		next = nal;
		while (next + 3 < size &&
		       !(au[next] == 0 && au[next + 1] == 0 &&
			 (au[next + 2] == 1 || (au[next + 2] == 0 && next + 4 <= size && au[next + 3] == 1))))
			next++;
		if (next + 3 >= size)
			next = size;
		type = (au[nal] >> 1) & 0x3f;
		if (type < 48 && o + (next - sc) <= VDEC_INPUT_BYTES) {
			memcpy(dst + o, au + sc, (size_t)(next - sc));
			o += next - sc;
		}
		i = next;
	}
	if (o <= 0) {
		if (size > VDEC_INPUT_BYTES)
			return -1;
		memcpy(dst, au, (size_t)size);
		return size;
	}
	return o;
}

int
hwdec_feed(hwdec_t *h, const uint8_t *au, int au_size)
{
	unsigned slot_in, slot_fr;
	uint8_t *in;
	int size;
	SceVideodec2InputData input;
	SceVideodec2FrameBuffer frame;
	SceVideodec2OutputInfo out;
	int rc;
	if (!h || h->dead || !h->decoder || !au || au_size <= 0)
		return h && h->dead ? -1 : 0;
	if (au_size > VDEC_INPUT_BYTES)
		return -1;
	slot_in = h->ring % VDEC_INPUT_SLOTS;
	slot_fr = h->ring % VDEC_FRAME_SLOTS;
	h->ring++;
	in = (uint8_t *)h->input_mem + (size_t)slot_in * VDEC_INPUT_BYTES;
	size = au_size;
	if (h->hevc)
		size = hevc_keep(au, au_size, in);
	else
		memcpy(in, au, (size_t)au_size);
	if (size <= 0)
		return 0;
	memset(&input, 0, sizeof input);
	memset(&frame, 0, sizeof frame);
	memset(&out, 0, sizeof out);
	input.size = sizeof input;
	input.au = in;
	input.au_size = (uint64_t)size;
	input.dts = ~(uint64_t)0;
	frame.size = sizeof frame;
	frame.buffer = (uint8_t *)h->frame_mem + (size_t)slot_fr * h->frame_size;
	frame.buffer_size = h->frame_size;
	out.size = sizeof out;
	rc = p_decode(h->decoder, &input, &frame, &out);
	if (rc) {
		if (++h->errors >= 8 && h->pictures == 0) {
			h->dead = 1;
			return -1;
		}
		return 0;
	}
	h->errors = 0;
	if (!copy_picture(h, &out))
		return 0;
	h->pictures++;
	return 1;
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
