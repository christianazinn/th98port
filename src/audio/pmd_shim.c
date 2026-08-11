#include "pmd_shim.h"

#include "fmdriver/fmdriver.h"
#include "fmdriver/fmdriver_pmd.h"

#include <stdlib.h>
#include <string.h>

struct th03_pmd_handle {
	struct fmdriver_work work;
	struct driver_pmd driver;
	bool loaded;
	bool started;
};

struct th03_pmd_callbacks {
	void *context;
	th03_pmd_read_register_fn read_register;
	th03_pmd_write_register_fn write_register;
	th03_pmd_status_fn read_status;
};

/* Keep the callback bundle in the handle rather than casting function-pointer
 * types through data fields. */
struct th03_pmd_owned_handle {
	struct th03_pmd_handle base;
	struct th03_pmd_callbacks callbacks;
};

static struct th03_pmd_owned_handle *owned(struct th03_pmd_handle *handle)
{
	return (struct th03_pmd_owned_handle *)handle;
}

/* The three driver callbacks receive the work pointer, so its existing opaque
 * OPNA slot locates the owning handle. No global shim state is necessary. */
static unsigned callback_read_register(
	struct fmdriver_work *work,
	unsigned address
)
{
	struct th03_pmd_owned_handle *handle =
		(struct th03_pmd_owned_handle *)work->opna;
	return handle->callbacks.read_register(
		handle->callbacks.context,
		address
	);
}

static void callback_write_register(
	struct fmdriver_work *work,
	unsigned address,
	unsigned value
)
{
	struct th03_pmd_owned_handle *handle =
		(struct th03_pmd_owned_handle *)work->opna;
	handle->callbacks.write_register(
		handle->callbacks.context,
		address,
		value
	);
}

static uint8_t callback_read_status(
	struct fmdriver_work *work,
	bool upper_port
)
{
	struct th03_pmd_owned_handle *handle =
		(struct th03_pmd_owned_handle *)work->opna;
	return handle->callbacks.read_status(
		handle->callbacks.context,
		upper_port
	);
}

static void silence_song(struct th03_pmd_handle *handle)
{
	unsigned bank;
	unsigned slot;
	unsigned channel;
	if(!handle || !handle->started) {
		return;
	}

	/* Reconstruct the public PMD stop/pause mute sequence retained in the
	 * imported driver as pmd_stop_sound(), including the rhythm dump present in
	 * the complete PMD service. Effects are not active in this parcel, so all
	 * six FM channels belong to the song. */
	for(bank = 0U; bank < 2U; ++bank) {
		for(slot = 0U; slot < 4U; ++slot) {
			for(channel = 0U; channel < 3U; ++channel) {
				handle->work.opna_writereg(
					&handle->work,
					(bank << 8U) | 0x80U | channel | (slot << 2U),
					0xFFU
				);
			}
		}
	}
	for(channel = 0U; channel < 3U; ++channel) {
		handle->work.opna_writereg(&handle->work, 0x28U, channel);
		handle->work.opna_writereg(&handle->work, 0x28U, 0x04U | channel);
	}
	handle->work.opna_writereg(&handle->work, 0x07U, 0xBFU);
	handle->work.opna_writereg(&handle->work, 0x10U, 0xFFU);
	if(!handle->driver.adpcm_disabled) {
		handle->work.opna_writereg(&handle->work, 0x101U, 0x02U);
		handle->work.opna_writereg(&handle->work, 0x100U, 0x01U);
		handle->work.opna_writereg(&handle->work, 0x110U, 0x80U);
		handle->work.opna_writereg(&handle->work, 0x110U, 0x18U);
	}
}

static struct pmd_part *service_part(
	struct th03_pmd_handle *handle,
	uint8_t part
)
{
	if(part <= 13U) {
		return &handle->driver.parts[part];
	}
	if(part == 15U) {
		return &handle->driver.parts[PMD_PART_FM_EFF];
	}
	if(part >= 16U && part <= 23U) {
		return &handle->driver.parts[PMD_PART_PPZ_1 + (part - 16U)];
	}
	return NULL;
}

static const struct pmd_part *service_part_const(
	const struct th03_pmd_handle *handle,
	uint8_t part
)
{
	if(part <= 13U) {
		return &handle->driver.parts[part];
	}
	if(part == 15U) {
		return &handle->driver.parts[PMD_PART_FM_EFF];
	}
	if(part >= 16U && part <= 23U) {
		return &handle->driver.parts[PMD_PART_PPZ_1 + (part - 16U)];
	}
	return NULL;
}

static bool part_is_masked(const struct pmd_part *part)
{
	return part->mask.ext || part->mask.effect || part->mask.disabled ||
		part->mask.slot || part->mask.mml || part->mask.ff;
}

struct th03_pmd_handle *th03_pmd_create(void)
{
	struct th03_pmd_owned_handle *handle =
		(struct th03_pmd_owned_handle *)calloc(1U, sizeof(*handle));
	return handle ? &handle->base : NULL;
}

void th03_pmd_destroy(struct th03_pmd_handle *handle)
{
	free(owned(handle));
}

bool th03_pmd_load(
	struct th03_pmd_handle *handle,
	uint8_t *song,
	uint16_t song_size
)
{
	if(!handle || !song || song_size == 0U) {
		return false;
	}
	memset(&handle->work, 0, sizeof(handle->work));
	memset(&handle->driver, 0, sizeof(handle->driver));
	handle->loaded = pmd_load(&handle->driver, song, song_size);
	handle->started = false;
	return handle->loaded;
}

bool th03_pmd_start(
	struct th03_pmd_handle *handle,
	void *context,
	th03_pmd_read_register_fn read_register,
	th03_pmd_write_register_fn write_register,
	th03_pmd_status_fn read_status
)
{
	struct th03_pmd_owned_handle *owner;
	if(!handle || !handle->loaded || !read_register || !write_register ||
		!read_status) {
		return false;
	}
	owner = owned(handle);
	owner->callbacks.context = context;
	owner->callbacks.read_register = read_register;
	owner->callbacks.write_register = write_register;
	owner->callbacks.read_status = read_status;

	memset(&handle->work, 0, sizeof(handle->work));
	handle->work.opna_readreg = callback_read_register;
	handle->work.opna_writereg = callback_write_register;
	handle->work.opna_status = callback_read_status;
	handle->work.opna = owner;
	handle->work.ppz8 = NULL;
	handle->work.ppz8_functbl = NULL;
	pmd_init(&handle->work, &handle->driver);
	handle->started = true;
	return true;
}

void th03_pmd_interrupt(struct th03_pmd_handle *handle)
{
	if(!handle || !handle->started || !handle->work.driver_opna_interrupt) {
		return;
	}
	if(handle->driver.paused) {
		/* A paused PMD still has to acknowledge the OPNA timers. Sequencer,
		 * measure, and fade state remain frozen until Resume. */
		while(handle->work.opna_status(&handle->work, false) & 3U) {
			handle->work.opna_writereg(
				&handle->work,
				0x27U,
				handle->driver.fm3ex_mode
			);
		}
		return;
	}
	handle->work.driver_opna_interrupt(&handle->work);
}

void th03_pmd_stop(struct th03_pmd_handle *handle)
{
	if(!handle || !handle->started) {
		return;
	}
	silence_song(handle);
	handle->driver.playing = false;
	handle->driver.paused = false;
	handle->driver.fadeout_speed = 0U;
	handle->driver.fadeout_vol = 0xFFU;
	handle->driver.status2 = 0xFFU;
	handle->work.playing = false;
	handle->work.paused = false;
}

void th03_pmd_pause(struct th03_pmd_handle *handle)
{
	if(!handle || !handle->started || !handle->driver.playing ||
		handle->driver.paused) {
		return;
	}
	silence_song(handle);
	handle->driver.paused = true;
	handle->work.paused = true;
}

void th03_pmd_resume(struct th03_pmd_handle *handle)
{
	if(!handle || !handle->started || !handle->driver.playing) {
		return;
	}
	handle->driver.paused = false;
	handle->work.paused = false;
}

void th03_pmd_fade(struct th03_pmd_handle *handle, int8_t speed)
{
	if(handle && handle->started && handle->driver.playing) {
		handle->driver.fadeout_speed = (uint8_t)speed;
	}
}

void th03_pmd_set_volume(struct th03_pmd_handle *handle, uint8_t volume)
{
	if(handle && handle->started && handle->driver.playing) {
		handle->driver.fadeout_vol = volume;
	}
}

bool th03_pmd_set_part_mask(
	struct th03_pmd_handle *handle,
	uint8_t part,
	bool masked
)
{
	struct pmd_part *selected;
	if(!handle || !handle->started || part > 23U) {
		return false;
	}
	if(part == 14U) {
		handle->driver.opnarhythm_mask = masked ? 0U : 0xFFU;
		return true;
	}
	selected = service_part(handle, part);
	if(!selected) {
		return false;
	}
	selected->mask.ext = masked;
	selected->proc_masked = part_is_masked(selected);
	return true;
}

bool th03_pmd_is_playing(const struct th03_pmd_handle *handle)
{
	return handle && handle->started && handle->work.playing;
}

bool th03_pmd_is_paused(const struct th03_pmd_handle *handle)
{
	return handle && handle->started && handle->driver.paused;
}

uint8_t th03_pmd_timer_b(const struct th03_pmd_handle *handle)
{
	return handle ? handle->work.timerb : 0U;
}

uint32_t th03_pmd_timer_b_count(const struct th03_pmd_handle *handle)
{
	return handle ? handle->work.timerb_cnt : 0U;
}

uint8_t th03_pmd_loop_count(const struct th03_pmd_handle *handle)
{
	return handle ? handle->work.loop_cnt : 0U;
}

uint16_t th03_pmd_measure(const struct th03_pmd_handle *handle)
{
	return handle ? handle->driver.meas_cnt : 0U;
}

uint8_t th03_pmd_fade_volume(const struct th03_pmd_handle *handle)
{
	return handle ? handle->driver.fadeout_vol : 0U;
}

int8_t th03_pmd_fade_speed(const struct th03_pmd_handle *handle)
{
	const uint8_t speed = handle ? handle->driver.fadeout_speed : 0U;
	return speed <= 0x7FU ? (int8_t)speed : (int8_t)((int)speed - 0x100);
}

uint32_t th03_pmd_part_mask(const struct th03_pmd_handle *handle)
{
	uint32_t result = 0U;
	uint8_t part;
	if(!handle || !handle->started) {
		return 0U;
	}
	for(part = 0U; part <= 23U; ++part) {
		const struct pmd_part *selected;
		bool masked;
		if(part == 14U) {
			masked = handle->driver.opnarhythm_mask == 0U;
		} else {
			selected = service_part_const(handle, part);
			masked = selected && selected->mask.ext;
		}
		if(masked) {
			result |= (uint32_t)1U << part;
		}
	}
	return result;
}
