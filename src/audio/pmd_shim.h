#ifndef TH03_AUDIO_PMD_SHIM_H
#define TH03_AUDIO_PMD_SHIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct th03_pmd_handle;

typedef unsigned (*th03_pmd_read_register_fn)(void *context, unsigned address);
typedef void (*th03_pmd_write_register_fn)(
	void *context,
	unsigned address,
	unsigned value
);
typedef uint8_t (*th03_pmd_status_fn)(void *context, bool upper_port);

struct th03_pmd_handle *th03_pmd_create(void);
void th03_pmd_destroy(struct th03_pmd_handle *handle);

/* The song image must remain mutable and alive until the next load/destroy. */
bool th03_pmd_load(
	struct th03_pmd_handle *handle,
	uint8_t *song,
	uint16_t song_size
);
bool th03_pmd_start(
	struct th03_pmd_handle *handle,
	void *context,
	th03_pmd_read_register_fn read_register,
	th03_pmd_write_register_fn write_register,
	th03_pmd_status_fn read_status
);
void th03_pmd_interrupt(struct th03_pmd_handle *handle);

void th03_pmd_stop(struct th03_pmd_handle *handle);
void th03_pmd_pause(struct th03_pmd_handle *handle);
void th03_pmd_resume(struct th03_pmd_handle *handle);
void th03_pmd_fade(struct th03_pmd_handle *handle, int8_t speed);
void th03_pmd_set_volume(struct th03_pmd_handle *handle, uint8_t volume);
bool th03_pmd_set_part_mask(
	struct th03_pmd_handle *handle,
	uint8_t part,
	bool masked
);

bool th03_pmd_is_playing(const struct th03_pmd_handle *handle);
bool th03_pmd_is_paused(const struct th03_pmd_handle *handle);
uint8_t th03_pmd_timer_b(const struct th03_pmd_handle *handle);
uint32_t th03_pmd_timer_b_count(const struct th03_pmd_handle *handle);
uint8_t th03_pmd_loop_count(const struct th03_pmd_handle *handle);
uint16_t th03_pmd_measure(const struct th03_pmd_handle *handle);
uint8_t th03_pmd_fade_volume(const struct th03_pmd_handle *handle);
int8_t th03_pmd_fade_speed(const struct th03_pmd_handle *handle);
uint32_t th03_pmd_part_mask(const struct th03_pmd_handle *handle);

#ifdef __cplusplus
}
#endif

#endif
