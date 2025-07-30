#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/rtc.h"

#include "ssd1306.h"
#include "font.h"
#include "ff.h"
#include "diskio.h"
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "rtc.h"
#include "sd_card.h"

#define SENSOR_BUS i2c0
#define SENSOR_DATA_PIN 0
#define SENSOR_CLOCK_PIN 1
static int SENSOR_ADDRESS = 0x68;

#define SCREEN_BUS i2c1
#define SCREEN_DATA_PIN 14
#define SCREEN_CLOCK_PIN 15
#define SCREEN_ADDRESS 0x3C
ssd1306_t display;

#define SWITCH_PRIMARY 5
#define SWITCH_SECONDARY 6
#define SWITCH_JOYSTICK 22
#define LIGHT_GREEN 11
#define LIGHT_BLUE 12
#define LIGHT_RED 13
#define SOUND_PRIMARY 21
#define SOUND_SECONDARY 10

char data_filename[20] = "sensor_log1.csv";
volatile int file_counter = 1;

volatile bool switch_primary_locked = true;
volatile bool switch_secondary_locked = true;

volatile bool mount_card_flag = false;
volatile bool unmount_card_flag = false;
volatile bool format_card_flag = false;
volatile bool record_data_flag = false;
volatile bool light_blink_flag = false;

volatile int current_screen;

static volatile uint32_t last_click_time = 0;

volatile int16_t motion_data[3];
volatile int16_t rotation_data[3];
volatile int16_t heat_reading;

volatile bool recording_active = false;
volatile int sample_count;
volatile float elapsed_time;

void refresh_screen(int screen_id, int message_id);
void activate_sound(int duration, int repetitions);
void set_light_color(bool red, bool green, bool blue);
void blink_light(int red, int green, int blue);

static void sensor_reset() {
    uint8_t reset_data[] = {0x6B, 0x80};
    i2c_write_blocking(SENSOR_BUS, SENSOR_ADDRESS, reset_data, 2, false);
    sleep_ms(100);
    
    reset_data[1] = 0x00;
    i2c_write_blocking(SENSOR_BUS, SENSOR_ADDRESS, reset_data, 2, false);
    sleep_ms(10);
}

static void sensor_read_data(volatile int16_t motion[3], volatile int16_t rotation[3], volatile int16_t *heat) {
    uint8_t data_buffer[6];

    uint8_t reg_addr = 0x3B;
    i2c_write_blocking(SENSOR_BUS, SENSOR_ADDRESS, &reg_addr, 1, true);
    i2c_read_blocking(SENSOR_BUS, SENSOR_ADDRESS, data_buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        motion[i] = (data_buffer[i * 2] << 8 | data_buffer[(i * 2) + 1]);
    }

    reg_addr = 0x43;
    i2c_write_blocking(SENSOR_BUS, SENSOR_ADDRESS, &reg_addr, 1, true);
    i2c_read_blocking(SENSOR_BUS, SENSOR_ADDRESS, data_buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        rotation[i] = (data_buffer[i * 2] << 8 | data_buffer[(i * 2) + 1]);
    }

    reg_addr = 0x41;
    i2c_write_blocking(SENSOR_BUS, SENSOR_ADDRESS, &reg_addr, 1, true);
    i2c_read_blocking(SENSOR_BUS, SENSOR_ADDRESS, data_buffer, 2, false);

    *heat = data_buffer[0] << 8 | data_buffer[1];
}

static sd_card_t *get_card_by_name(const char *const name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static FATFS *get_filesystem_by_name(const char *name)
{
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

static void execute_format()
{
    switch_primary_locked = true;
    switch_secondary_locked = true;
    activate_sound(300, 1);
    blink_light(1, 0, 1);

    const char *drive_name = strtok(NULL, " ");
    if (!drive_name)
        drive_name = sd_get_by_num(0)->pcName;
    FATFS *filesystem = get_filesystem_by_name(drive_name);
    if (!filesystem)
    {
        printf("Unknown logical drive number: \"%s\"\n", drive_name);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(3, 3);
        activate_sound(300, 3);
        light_blink_flag = false;
        return;
    }
    FRESULT result = f_mkfs(drive_name, 0, 0, FF_MAX_SS * 2);
    if (FR_OK != result)
    {
        printf("f_mkfs error: %s (%d)\n", FRESULT_str(result), result);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(3, 3);
        activate_sound(300, 3);
        light_blink_flag = false;
        return;
    }
    switch_primary_locked = false;
    switch_secondary_locked = false;
    refresh_screen(3, 4);
    activate_sound(300, 2);
    light_blink_flag = false;
}

static void execute_mount()
{
    switch_primary_locked = true;
    switch_secondary_locked = true;
    activate_sound(200, 1);
    blink_light(1, 0, 1);

    const char *drive_name = strtok(NULL, " ");
    if (!drive_name)
        drive_name = sd_get_by_num(0)->pcName;
    FATFS *filesystem = get_filesystem_by_name(drive_name);
    if (!filesystem)
    {
        printf("Unknown logical drive number: \"%s\"\n", drive_name);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(2, 4);
        activate_sound(200, 3);
        light_blink_flag = false;
        return;
    }
    FRESULT result = f_mount(filesystem, drive_name, 1);
    if (FR_OK != result)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(result), result);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(2, 4);
        activate_sound(200, 3);
        light_blink_flag = false;
        return;
    }
    sd_card_t *card = get_card_by_name(drive_name);
    myASSERT(card);
    card->mounted = true;
    printf("SD card mount process ( %s ) completed\n", card->pcName);
    switch_primary_locked = false;
    switch_secondary_locked = false;
    refresh_screen(2, 1);
    activate_sound(200, 2);
    light_blink_flag = false;
}

static void execute_unmount()
{
    switch_primary_locked = true;
    switch_secondary_locked = true;
    activate_sound(200, 1);
    blink_light(1, 0, 1);

    const char *drive_name = strtok(NULL, " ");
    if (!drive_name)
        drive_name = sd_get_by_num(0)->pcName;
    FATFS *filesystem = get_filesystem_by_name(drive_name);
    if (!filesystem)
    {
        printf("Unknown logical drive number: \"%s\"\n", drive_name);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(2, 4);
        activate_sound(200, 3);
        light_blink_flag = false;
        return;
    }
    FRESULT result = f_unmount(drive_name);
    if (FR_OK != result)
    {
        printf("f_unmount error: %s (%d)\n", FRESULT_str(result), result);
        switch_primary_locked = false;
        switch_secondary_locked = false;
        refresh_screen(2, 4);
        activate_sound(200, 3);
        light_blink_flag = false;
        return;
    }
    sd_card_t *card = get_card_by_name(drive_name);
    myASSERT(card);
    card->mounted = false;
    card->m_Status |= STA_NOINIT;
    printf("SD ( %s ) unmounted\n", card->pcName);

    switch_primary_locked = false;
    switch_secondary_locked = false;
    refresh_screen(2, 1);
    activate_sound(200, 2);
    light_blink_flag = false;
}

bool is_card_mounted()
{
    const char *drive_name = strtok(NULL, " ");
    if (!drive_name)
        drive_name = sd_get_by_num(0)->pcName;

    sd_card_t *card = get_card_by_name(drive_name);
    if (!card) {
        printf("SD card with name \"%s\" not found\n", drive_name);
        return false;
    }

    return card->mounted;
}

void set_pwm_sound(uint gpio, bool active);
int64_t sound_off_alarm(alarm_id_t id, void *user_data);

int64_t sound_on_alarm(alarm_id_t id, void *user_data){
    uintptr_t packed_on = (uintptr_t)user_data;
    int reps = (packed_on >> 16) & 0xFFFF;
    int duration = packed_on & 0xFFFF;
    
    set_pwm_sound(SOUND_PRIMARY, true);
    set_pwm_sound(SOUND_SECONDARY, true);
    
    uintptr_t packed_off = ((uintptr_t)reps << 16) | (duration & 0xFFFF);
    add_alarm_in_ms(duration, sound_off_alarm, (void *)packed_off, false);
    return 0;
}

int64_t sound_off_alarm(alarm_id_t id, void *user_data){
    uintptr_t packed_off = (uintptr_t)user_data;
    int reps = (packed_off >> 16) & 0xFFFF;
    int duration = packed_off & 0xFFFF;
    
    set_pwm_sound(SOUND_PRIMARY, false);
    set_pwm_sound(SOUND_SECONDARY, false);
    
    reps -= 1;
    
    if(reps > 0){
        uintptr_t packed_on = ((uintptr_t)reps << 16) | (duration & 0xFFFF);
        add_alarm_in_ms(duration, sound_on_alarm, (void *)packed_on, false);
    }
    return 0;
}

void set_pwm_frequency(uint gpio, uint frequency) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint clock_divider = 4;
    uint wrap_value = (125000000 / (clock_divider * frequency)) - 1;

    pwm_set_clkdiv(slice_num, clock_divider);
    pwm_set_wrap(slice_num, wrap_value);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), wrap_value / 40);
}

void set_pwm_sound(uint gpio, bool active){
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_enabled(slice_num, active);
}

void activate_sound(int duration, int repetitions){
    uintptr_t packed_on = ((uintptr_t)repetitions << 16) | (duration & 0xFFFF);
    add_alarm_in_ms(20, sound_on_alarm, (void *)packed_on, false);
}

int64_t light_off_alarm(alarm_id_t id, void *user_data);

int64_t light_on_alarm(alarm_id_t id, void *user_data){
    uintptr_t light_on = (uintptr_t)user_data;

    int red = (light_on >> 2) & 1;
    int green = (light_on >> 1) & 1;
    int blue = (light_on >> 0) & 1;

    set_light_color(red, green, blue);

    uintptr_t light_off = (red << 2) | (green << 1) | blue;
    add_alarm_in_ms(200, light_off_alarm, (void *)light_off, false);

    return 0;
}

int64_t light_off_alarm(alarm_id_t id, void *user_data){
    uintptr_t light_off = (uintptr_t)user_data;

    int red = (light_off >> 2) & 1;
    int green = (light_off >> 1) & 1;
    int blue = (light_off >> 0) & 1;

    set_light_color(0, 0, 0);

    if(light_blink_flag){
        uintptr_t light_on = (red << 2) | (green << 1) | blue;
        add_alarm_in_ms(200, light_on_alarm, (void *)light_on, false);
    }

    return 0;
}

void blink_light(int red, int green, int blue){
    light_blink_flag = true;
    uintptr_t light_on = (red << 2) | (green << 1) | blue;
    add_alarm_in_ms(20, light_on_alarm, (void *)light_on, false);
}

void set_light_color(bool red, bool green, bool blue){
    gpio_put(LIGHT_RED, red);
    gpio_put(LIGHT_GREEN, green);
    gpio_put(LIGHT_BLUE, blue);
}

void refresh_screen(int screen_id, int message_id){
    ssd1306_fill(&display, false);
    ssd1306_rect(&display, 0, 0, 127, 63, true, false);
    current_screen = screen_id;

    if(screen_id == 1){
        ssd1306_draw_string(&display, "EMBEDDED LOGG", 8, 3);

        ssd1306_line(&display, 1, 12, 126, 12, true);

        if(message_id == 1){
            ssd1306_draw_string(&display, "Starting...", 16, 27);
        }else if(message_id == 2){
            ssd1306_draw_string(&display, "System ready!", 8, 15);

            ssd1306_line(&display, 1, 24, 126, 24, true);

            ssd1306_draw_string(&display, "Press A to:", 4, 30);
            ssd1306_draw_string(&display, "Change screen", 12, 39);
        }

        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "Screen 1 of 5", 20, 53);

    }else if(screen_id == 2){
        ssd1306_draw_string(&display, "SD Card", 28, 3);

        ssd1306_line(&display, 1, 12, 126, 12, true);
        if(message_id == 1){
            if(is_card_mounted()){
                ssd1306_draw_string(&display, "Mounted!", 32, 15);
                ssd1306_draw_string(&display, "Unmount", 28, 39);
            }else{
                ssd1306_draw_string(&display, "Not mounted!", 16, 15);
                ssd1306_draw_string(&display, "Mount", 40, 39);
            }
            ssd1306_draw_string(&display, "Press B to:", 4, 30);
        }else if(message_id == 2){
            ssd1306_draw_string(&display, "Unmounting...", 8, 15);
            ssd1306_draw_string(&display, "Please wait...", 24, 33);
        }else if(message_id == 3){
            ssd1306_draw_string(&display, "Mounting...", 20, 15);
            ssd1306_draw_string(&display, "Please wait...", 24, 33);
        }else if(message_id == 4){
            ssd1306_draw_string(&display, "Error!", 44, 15);
            ssd1306_draw_string(&display, "Press B to:", 4, 30);
            ssd1306_draw_string(&display, "Try again", 8, 39);
        }

        ssd1306_line(&display, 1, 24, 126, 24, true);

        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "Screen 2 of 5", 20, 53);

    }else if(screen_id == 3){
       ssd1306_draw_string(&display, "Format", 24, 3);

       
       if(message_id == 1){
            ssd1306_draw_string(&display, "SD card", 16, 13);
            ssd1306_draw_string(&display, "Press B to:", 4, 30);
            ssd1306_draw_string(&display, "Format", 32, 39);
        }else if(message_id == 2){
            ssd1306_line(&display, 1, 12, 126, 12, true);

            ssd1306_draw_string(&display, "Formatting...", 12, 15);
            ssd1306_draw_string(&display, "Please wait...", 24, 33);
        }else if(message_id == 3){
            ssd1306_line(&display, 1, 12, 126, 12, true);

            ssd1306_draw_string(&display, "Error!", 44, 15);
            ssd1306_draw_string(&display, "Press B to:", 4, 30);
            ssd1306_draw_string(&display, "Try again", 8, 39);
        }else if(message_id == 4){
            ssd1306_line(&display, 1, 12, 126, 12, true);

            ssd1306_draw_string(&display, "SD Formatted!", 12, 15);
            ssd1306_draw_string(&display, "Press B to:", 4, 30);
            ssd1306_draw_string(&display, "Format", 32, 39);
        }

        ssd1306_line(&display, 1, 24, 126, 24, true);

        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "Screen 3 of 5", 20, 53);

    }else if(screen_id == 4){
        ssd1306_draw_string(&display, "Record data", 16, 3);

        ssd1306_line(&display, 1, 12, 126, 12, true);

        if(message_id == 1){
            ssd1306_draw_string(&display, "Press B to:", 4, 16);
            ssd1306_draw_string(&display, "Record to", 28, 27);
            ssd1306_draw_string(&display, data_filename, 12, 38);
        }else if(message_id == 2){
            ssd1306_draw_string(&display, "Data recorded!", 4, 3);
            ssd1306_draw_string(&display, data_filename, 12, 15);

            ssd1306_line(&display, 1, 24, 126, 24, true);

            ssd1306_draw_string(&display, "Press B to:", 4, 30);
            ssd1306_draw_string(&display, "Record again", 8, 39);
        }else if(message_id == 3){
            ssd1306_draw_string(&display, "Error!", 44, 15);

            ssd1306_line(&display, 1, 24, 126, 24, true);

            ssd1306_draw_string(&display, "Mount card", 8, 30);
            ssd1306_draw_string(&display, "to record!", 24, 39);
        }

        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "Screen 4 of 5", 20, 53);
    }else if(screen_id == 5){
        ssd1306_draw_string(&display, "MPU 6050", 32, 3);

        ssd1306_line(&display, 1, 12, 126, 12, true);

        int gyro_center_x = 44;
        int gyro_max_width = gyro_center_x - 10;

        int gyro_y_gx = 21;
        int gyro_y_gy = 31;
        int gyro_y_gz = 41;

        float gyro_scale = (float)gyro_max_width / 32767.0f;

        float last_gx, last_gy, last_gz;
        const float gyro_threshold = 100.0f;

        float gx = (float)rotation_data[0];
        float gy = (float)rotation_data[1];
        float gz = (float)rotation_data[2];

        last_gx = gx;
        last_gy = gy;
        last_gz = gz;

        int gyro_end_x_gx = gyro_center_x + (int)(gx * gyro_scale);
        if (gyro_end_x_gx < 0)
            gyro_end_x_gx = 0;
        if (gyro_end_x_gx > (gyro_center_x * 2) - 1)
            gyro_end_x_gx = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gx, gyro_end_x_gx, gyro_y_gx, true);

        int gyro_end_x_gy = gyro_center_x + (int)(gy * gyro_scale);
        if (gyro_end_x_gy < 0)
            gyro_end_x_gy = 0;
        if (gyro_end_x_gy > (gyro_center_x * 2) - 1)
            gyro_end_x_gy = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gy, gyro_end_x_gy, gyro_y_gy, true);

        int gyro_end_x_gz = gyro_center_x + (int)(gz * gyro_scale);
        if (gyro_end_x_gz < 0)
            gyro_end_x_gz = 0;
        if (gyro_end_x_gz > (gyro_center_x * 2) - 1)
            gyro_end_x_gz = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gz, gyro_end_x_gz, gyro_y_gz, true);

        int accel_center_x = 108;
        int accel_ref_y = 31;

        float pixels_per_degree_roll = 0.8f;
        float pixels_per_degree_pitch = 0.8f;

        int accel_line_length = 25;

        float last_roll, last_pitch;
        const float accel_threshold = 1.0f;

        float ax = motion_data[0] / 16384.0f;
        float ay = motion_data[1] / 16384.0f;
        float az = motion_data[2] / 16384.0f;

        float roll = atan2(ay, az) * 180.0f / M_PI;
        float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;

        last_roll = roll;
        last_pitch = pitch;

        int roll_offset_x = (int)(roll * pixels_per_degree_roll);
        int roll_line_x = accel_center_x + roll_offset_x;

        if (roll_line_x < 89)
            roll_line_x = 89;
        if (roll_line_x > 125)
            roll_line_x = 125;

        int pitch_offset_y = (int)(pitch * pixels_per_degree_pitch);
        int pitch_line_y = accel_ref_y + pitch_offset_y;

        if (pitch_line_y < 13)
            pitch_line_y = 13;
        if (pitch_line_y > 50)
            pitch_line_y = 50;

        ssd1306_line(&display, accel_center_x - 2, accel_ref_y, accel_center_x + 2, accel_ref_y, true);
        ssd1306_line(&display, accel_center_x, accel_ref_y - 2, accel_center_x, accel_ref_y + 2, true);

        ssd1306_line(&display, roll_line_x, accel_ref_y - (accel_line_length / 2) + 5, roll_line_x, accel_ref_y + (accel_line_length / 2) - 5, true);

        ssd1306_line(&display, accel_center_x - (accel_line_length / 2) + 5, pitch_line_y, accel_center_x + (accel_line_length / 2) - 5, pitch_line_y, true);

        ssd1306_line(&display, 88, 12, 88, 51, true);

        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "Screen 5 of 5", 20, 53);

    }else if(screen_id == 6){
        ssd1306_draw_string(&display, "Recording...", 20, 3);

        ssd1306_line(&display, 1, 12, 126, 12, true);

        ssd1306_draw_string(&display, "T:", 4, 14);

        char time_str[5];
        if(elapsed_time > 9.9){
            sprintf(time_str, "%.1fs", elapsed_time);
        }else{
            sprintf(time_str, "0%.1fs", elapsed_time);
        }
        ssd1306_draw_string(&display, time_str, 21, 14);

        ssd1306_line(&display, 63, 12, 63, 23, true);

        ssd1306_draw_string(&display, "N:", 67, 14);

        char count_str[5];
        if(sample_count > 99){
            sprintf(count_str, "%d", sample_count);
        }else if(sample_count > 9){
            sprintf(count_str, "0%d", sample_count);
        }else{
            sprintf(count_str, "00%d", sample_count);
        }
        ssd1306_draw_string(&display, count_str, 93, 14);
        
        ssd1306_line(&display, 1, 23, 126, 23, true);

        int gyro_center_x = 50;
        int gyro_max_width = gyro_center_x - 10;

        int gyro_y_gx = 30;
        int gyro_y_gy = 37;
        int gyro_y_gz = 44;

        float gyro_scale = (float)gyro_max_width / 32767.0f;

        float last_gx, last_gy, last_gz;
        const float gyro_threshold = 100.0f;

        float gx = (float)rotation_data[0];
        float gy = (float)rotation_data[1];
        float gz = (float)rotation_data[2];

        last_gx = gx;
        last_gy = gy;
        last_gz = gz;

        int gyro_end_x_gx = gyro_center_x + (int)(gx * gyro_scale);
        if (gyro_end_x_gx < 0)
            gyro_end_x_gx = 0;
        if (gyro_end_x_gx > (gyro_center_x * 2) - 1)
            gyro_end_x_gx = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gx, gyro_end_x_gx, gyro_y_gx, true);

        int gyro_end_x_gy = gyro_center_x + (int)(gy * gyro_scale);
        if (gyro_end_x_gy < 0)
            gyro_end_x_gy = 0;
        if (gyro_end_x_gy > (gyro_center_x * 2) - 1)
            gyro_end_x_gy = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gy, gyro_end_x_gy, gyro_y_gy, true);

        int gyro_end_x_gz = gyro_center_x + (int)(gz * gyro_scale);
        if (gyro_end_x_gz < 0)
            gyro_end_x_gz = 0;
        if (gyro_end_x_gz > (gyro_center_x * 2) - 1)
            gyro_end_x_gz = (gyro_center_x * 2) - 1;
        ssd1306_line(&display, gyro_center_x, gyro_y_gz, gyro_end_x_gz, gyro_y_gz, true);

        int accel_center_x = 113;
        int accel_ref_y = 37;

        float pixels_per_degree_roll = 0.8f;
        float pixels_per_degree_pitch = 0.8f;

        int accel_line_length = 20;

        float last_roll, last_pitch;
        const float accel_threshold = 1.0f;

        float ax = motion_data[0] / 16384.0f;
        float ay = motion_data[1] / 16384.0f;
        float az = motion_data[2] / 16384.0f;

        float roll = atan2(ay, az) * 180.0f / M_PI;
        float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;

        last_roll = roll;
        last_pitch = pitch;

        int roll_offset_x = (int)(roll * pixels_per_degree_roll);
        int roll_line_x = accel_center_x + roll_offset_x;

        if (roll_line_x < 100)
            roll_line_x = 100;
        if (roll_line_x > 125)
            roll_line_x = 125;

        int pitch_offset_y = (int)(pitch * pixels_per_degree_pitch);
        int pitch_line_y = accel_ref_y + pitch_offset_y;

        if (pitch_line_y < 24)
            pitch_line_y = 24;
        if (pitch_line_y > 50)
            pitch_line_y = 50;

        ssd1306_line(&display, accel_center_x - 2, accel_ref_y, accel_center_x + 2, accel_ref_y, true);
        ssd1306_line(&display, accel_center_x, accel_ref_y - 2, accel_center_x, accel_ref_y + 2, true);

        ssd1306_line(&display, roll_line_x, accel_ref_y - (accel_line_length / 2) + 5, roll_line_x, accel_ref_y + (accel_line_length / 2) - 5, true);

        ssd1306_line(&display, accel_center_x - (accel_line_length / 2) + 5, pitch_line_y, accel_center_x + (accel_line_length / 2) - 5, pitch_line_y, true);

        ssd1306_line(&display, 99, 23, 99, 51, true);
        ssd1306_line(&display, 1, 51, 126, 51, true);

        ssd1306_draw_string(&display, "B to stop", 24, 53);

    }
    ssd1306_send_data(&display);
}

void store_sensor_data(){
    switch_primary_locked = true;
    activate_sound(100, 1);
    blink_light(0, 0, 1);

    FIL data_file;
    FRESULT result = f_open(&data_file, data_filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK)
    {
        printf("\n[ERROR] Could not open file for writing. Mount the card.\n");
        switch_primary_locked = false;
        refresh_screen(4, 3);
        activate_sound(100, 3);
        light_blink_flag = false;
        return;
    }
    recording_active = true;

    char data_buffer[80];
    sprintf(data_buffer, "sample_number,time_s,motion_x,motion_y,motion_z,rotation_x,rotation_y,rotation_z\n");   
    UINT bytes_written;
    result = f_write(&data_file, data_buffer, strlen(data_buffer), &bytes_written);
    if (result != FR_OK)
    {
        printf("[ERROR] Could not write to file. Mount the card.\n");
        switch_primary_locked = false;
        refresh_screen(4, 3);
        activate_sound(100, 3);
        light_blink_flag = false;
        f_close(&data_file);
        return;
    }
    while(recording_active){
        sensor_read_data(motion_data, rotation_data, &heat_reading);

        sprintf(data_buffer,"%d,%.1f,%d,%d,%d,%d,%d,%d\n", sample_count, elapsed_time, motion_data[0], motion_data[1], motion_data[2], rotation_data[0], rotation_data[1], rotation_data[2]);
        result = f_write(&data_file, data_buffer, strlen(data_buffer), &bytes_written);
        if (result != FR_OK)
        {
            printf("[ERROR] Could not write to file. Mount the card.\n");
            switch_primary_locked = false;
            refresh_screen(4, 3);
            activate_sound(100, 3);
            light_blink_flag = false;
            f_close(&data_file);
            return;
        }
        refresh_screen(6, 1);
        sleep_ms(100);
        sample_count += 1;
        elapsed_time += 0.1;
    }
    f_close(&data_file);
    printf("\nMPU data saved to file %s.\n\n", data_filename);
    switch_primary_locked = false;
    refresh_screen(4, 2);
    sample_count = 0;
    elapsed_time = 0.0;
    file_counter += 1;
    sprintf(data_filename, "sensor_log%d.csv", file_counter);
    activate_sound(100, 2);
    light_blink_flag = false;
}

void gpio_interrupt_handler(uint gpio, uint32_t events){
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if(current_time - last_click_time > 1000000){
        last_click_time = current_time;
        if(gpio == SWITCH_PRIMARY){
            if(!switch_primary_locked){
                if(current_screen >= 5){
                    refresh_screen(1, 2);
                }else{
                    current_screen += 1;
                    refresh_screen(current_screen, 1);
                }
            }
        }else if(gpio == SWITCH_SECONDARY){
            if(!switch_secondary_locked){
                if(current_screen == 2){
                    if(is_card_mounted()){
                        refresh_screen(2, 2);
                        unmount_card_flag = true;
                    } else {
                        refresh_screen(2, 3);
                        mount_card_flag = true;
                    }
                }else if(current_screen == 3){
                    refresh_screen(3, 2);
                    format_card_flag = true;
                }else if(current_screen == 4){
                    refresh_screen(6, 1);
                    record_data_flag = true;
                }else if(current_screen == 6){
                    recording_active = false;
                }
            }
        }
        else if(gpio == SWITCH_JOYSTICK){
            ssd1306_fill(&display, false);
            ssd1306_draw_string(&display, "BOOTSEL MODE", 16, 27);
            ssd1306_send_data(&display);

            reset_usb_boot(0, 0);
        }
    }
}

int main()
{
    stdio_init_all();

    gpio_init(LIGHT_GREEN);
    gpio_set_dir(LIGHT_GREEN, GPIO_OUT);
    gpio_init(LIGHT_BLUE);
    gpio_set_dir(LIGHT_BLUE, GPIO_OUT);
    gpio_init(LIGHT_RED);
    gpio_set_dir(LIGHT_RED, GPIO_OUT);

    gpio_set_function(SOUND_PRIMARY, GPIO_FUNC_PWM);
    gpio_set_function(SOUND_SECONDARY, GPIO_FUNC_PWM);
    set_pwm_frequency(SOUND_PRIMARY, 1000);
    set_pwm_frequency(SOUND_SECONDARY, 1000);

    gpio_init(SWITCH_PRIMARY);
    gpio_set_dir(SWITCH_PRIMARY, GPIO_IN);
    gpio_pull_up(SWITCH_PRIMARY);
    gpio_init(SWITCH_SECONDARY);
    gpio_set_dir(SWITCH_SECONDARY, GPIO_IN);
    gpio_pull_up(SWITCH_SECONDARY);
    gpio_init(SWITCH_JOYSTICK);
    gpio_set_dir(SWITCH_JOYSTICK, GPIO_IN);
    gpio_pull_up(SWITCH_JOYSTICK);
    gpio_set_irq_enabled_with_callback(SWITCH_PRIMARY, GPIO_IRQ_EDGE_FALL, true, &gpio_interrupt_handler);
    gpio_set_irq_enabled_with_callback(SWITCH_SECONDARY, GPIO_IRQ_EDGE_FALL, true, &gpio_interrupt_handler);
    gpio_set_irq_enabled_with_callback(SWITCH_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &gpio_interrupt_handler);

    i2c_init(SCREEN_BUS, 400 * 1000);
    gpio_set_function(SCREEN_DATA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCREEN_CLOCK_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SCREEN_DATA_PIN);
    gpio_pull_up(SCREEN_CLOCK_PIN);
    ssd1306_init(&display, WIDTH, HEIGHT, false, SCREEN_ADDRESS, SCREEN_BUS);
    ssd1306_config(&display);
    ssd1306_send_data(&display);
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    i2c_init(SENSOR_BUS, 400 * 1000);
    gpio_set_function(SENSOR_DATA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSOR_CLOCK_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SENSOR_DATA_PIN);
    gpio_pull_up(SENSOR_CLOCK_PIN);

    refresh_screen(1, 1);
    set_light_color(1, 1, 0);
    sleep_ms(2000);

    bi_decl(bi_2pins_with_func(SENSOR_DATA_PIN, SENSOR_CLOCK_PIN, GPIO_FUNC_I2C));
    sensor_reset();
    
    switch_primary_locked = false;
    switch_secondary_locked = false;
    refresh_screen(1, 2);
    while (true) {
        if(is_card_mounted()){
            set_light_color(0, 1, 0);
        }else{
            set_light_color(1, 0, 0);
        }
        
        if(mount_card_flag){
            mount_card_flag = false;
            execute_mount();
        }
        if(unmount_card_flag){
            unmount_card_flag = false;
            execute_unmount();
        }
        if(format_card_flag){
            format_card_flag = false;
            execute_format();
        }
        if(record_data_flag){
            record_data_flag = false;
            store_sensor_data();
        }
        if(current_screen == 5){
            sensor_read_data(motion_data, rotation_data, &heat_reading);
            refresh_screen(5, 1);
        }

        sleep_ms(100);
    }
    return 0;
}