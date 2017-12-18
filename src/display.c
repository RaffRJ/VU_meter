#include "display.h"
#include <stdlib.h>
#include <avr/pgmspace.h>
#include "utils.h"
#include "assert.h"


void
display_init(display_t *display, ssd1306_t *device)
{
  display->device = device;
  display->sprites_n = 0;
}


void
display_add_sprite(display_t *display, sprite_t *sprite)
{
  assert(display->sprites_n < DISPLAY_MAX_SPRITES);

  display->sprites[display->sprites_n] = sprite;
  ++display->sprites_n;
}

#define SEGMENTS_N (32)

bool
display_update_async_cb(display_t *display)
{
  ssd1306_segment_t segments[SEGMENTS_N];

  for (uint8_t i = 0; i < display->sprites_n; ++i) {
    if (display->sprites[i]->visible) {
      display->sprites[i]->render(
        display->sprites[i],
        display->update.full.column,
        display->update.full.page,
        display->update.full.column + SEGMENTS_N - 1,
        segments
      );
    }
  }

  ssd1306_put_segments(
    display->device,
    display->update.full.column,
    display->update.full.page,
    SEGMENTS_N,
    segments
  );

  display->update.full.column += SEGMENTS_N;

  if (display->update.full.column >= SSD1306_COLUMNS_N) {
    display->update.full.column = 0;
    ++display->update.full.page;

    if (display->update.full.page >= SSD1306_PAGES_N) {
      ssd1306_finish_update(display->device);
      return false;
    }
  }

  return true;
}


void
display_update_async(display_t *display)
{
  display->update.full.column = 0;
  display->update.full.page = 0;

  ssd1306_start_update(
    display->device,
    (ssd1306_update_callback_t) display_update_async_cb,
    display
  );
}


bool
display_update_partial_async_cb(display_t *display)
{
  ssd1306_segment_t segments[SEGMENTS_N];
  partial_update_ctrl_t *update = &(display->update.partial);
  region_t *region = &(update->extents->regions[update->region_index]);

  uint8_t column_b = int_min(update->column + SEGMENTS_N - 1, region->end_column);

  for (uint8_t i = 0; i < display->sprites_n; ++i) {
    if (display->sprites[i]->visible) {
      display->sprites[i]->render(
        display->sprites[i],
        update->column,
        region->page,
        column_b,
        segments
      );
    }
  }

  ssd1306_put_segments(
    display->device,
    update->column,
    region->page,
    column_b - update->column + 1,
    segments
  );

  if (column_b == region->end_column) {
    ++(update->region_index);

    if (update->region_index == update->extents->regions_n) {
      ssd1306_finish_update(display->device);
      return false;
    }

    update->column = update->extents->regions[update->region_index].start_column;
  }
  else {
    update->column = column_b + 1;
  }

  return true;
}


void
display_update_partial_async(display_t *display, update_extents_t *extents)
{
  display->update.partial.region_index = 0;
  display->update.partial.column = extents->regions[0].start_column;
  display->update.partial.extents = extents;

  ssd1306_start_update(
    display->device,
    (ssd1306_update_callback_t) display_update_partial_async_cb,
    display
  );
}


void
update_extents_reset(update_extents_t *extents)
{
  extents->regions_n = 0;
}


void
update_extents_add_region(update_extents_t *extents, uint8_t page, uint8_t start_column, uint8_t end_column)
{
  extents->regions[extents->regions_n].page = page;
  extents->regions[extents->regions_n].start_column = start_column;
  extents->regions[extents->regions_n].end_column = end_column;
  ++(extents->regions_n);
}


int
cmp_regions_by_page(const void *a, const void *b)
{
  region_t *region_a = (region_t *) a;
  region_t *region_b = (region_t *) b;

  return (region_a->page > region_b->page) - (region_a->page < region_b->page);
}


void
update_extents_optimize(update_extents_t *extents)
{
  qsort(extents->regions, extents->regions_n, sizeof(region_t), cmp_regions_by_page);
}
