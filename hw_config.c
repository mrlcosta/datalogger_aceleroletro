
#include <assert.h>
#include <string.h>
#include "my_debug.h"
#include "hw_config.h"
#include "ff.h"
#include "diskio.h"

static spi_t spi_controllers[] = {
    {
        .hw_inst = spi0,
        .miso_gpio = 16,
        .mosi_gpio = 19,
        .sck_gpio = 18,
        .baud_rate = 1000 * 1000
    }};

static sd_card_t storage_devices[] = {
    {
        .pcName = "0:",
        .spi = &spi_controllers[0],
        .ss_gpio = 17,
        .use_card_detect = false,
        .card_detect_gpio = 22,
        .card_detected_true = -1
    }};

size_t sd_get_num() { return count_of(storage_devices); }
sd_card_t *sd_get_by_num(size_t num) {
    assert(num <= sd_get_num());
    if (num <= sd_get_num()) {
        return &storage_devices[num];
    } else {
        return NULL;
    }
}
size_t spi_get_num() { return count_of(spi_controllers); }
spi_t *spi_get_by_num(size_t num) {
    assert(num <= spi_get_num());
    if (num <= spi_get_num()) {
        return &spi_controllers[num];
    } else {
        return NULL;
    }
}
