#ifndef __SELECTION_H__
#define __SELECTION_H__

#include <stdint.h>
#include <stdbool.h>

#include <events.h>

void *select_init();

bool select_event(uint32_t samples_number,
                  const uint16_t *samples,
                  size_t baseline_end,
                  uint64_t timestamp,
                  double qshort,
                  double qlong,
                  double baseline,
                  uint8_t channel,
                  uint8_t PUR,
                  struct event_PSD *event,
                  void *user_data);

int select_close(void *user_data);

#endif
