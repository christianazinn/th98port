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
	if(handle && handle->started && handle->work.driver_opna_interrupt) {
		handle->work.driver_opna_interrupt(&handle->work);
	}
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
