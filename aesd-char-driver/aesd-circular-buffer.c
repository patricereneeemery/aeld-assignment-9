/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer implementation
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
            struct aesd_circular_buffer *buffer,
            size_t char_offset,
            size_t *entry_offset_byte_rtn)
{
    size_t total = 0;
    uint8_t idx;
    uint8_t count;
    uint8_t i;

    if (buffer->full)
        count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    else
        count = buffer->in_offs;

    idx = buffer->out_offs;

    for (i = 0; i < count; i++) {
        if (char_offset < total + buffer->entry[idx].size) {
            *entry_offset_byte_rtn = char_offset - total;
            return &buffer->entry[idx];
        }
        total += buffer->entry[idx].size;
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    return NULL;
}

void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
                                    const struct aesd_buffer_entry *add_entry)
{
    buffer->entry[buffer->in_offs] = *add_entry;

    if (buffer->full) {
        buffer->out_offs =
            (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    buffer->in_offs =
        (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->in_offs == buffer->out_offs)
        buffer->full = true;
}

void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
