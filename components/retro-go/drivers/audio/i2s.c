#include "rg_system.h"
#include "rg_audio.h"

#if RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC

#ifndef ESP_PLATFORM
#error "I2S support can only be built inside esp-idf!"
#elif !CONFIG_IDF_TARGET_ESP32 && RG_AUDIO_USE_INT_DAC
#error "Your chip has no DAC! Please set RG_AUDIO_USE_INT_DAC to 0 in your target file."
#endif

#include <driver/gpio.h>
#include <driver/i2s.h>

#ifdef RG_GPIO_SND_AMP_ENABLE_INVERT
#define MUTE_ENABLE 1
#define MUTE_DISABLE 0
#else
#define MUTE_ENABLE 0
#define MUTE_DISABLE 1
#endif

// Fixed-rate output must size DMA in output frames, not emulator input frames.
// Six 5 ms descriptors leave 25 ms writable while one descriptor is playing:
// enough for a 50 Hz frame plus scheduling/rendering jitter. At 48 kHz the old
// 4 x 180 queue held only 15 ms and could replay stale audio between game frames.
#if RG_AUDIO_I2S_SAMPLE_RATE
#define DMA_BUFFER_COUNT 6
#define DMA_BUFFER_LEN ((RG_AUDIO_I2S_SAMPLE_RATE + 199) / 200)
#else
#define DMA_BUFFER_COUNT 4
#define DMA_BUFFER_LEN 180
#endif

static struct {
    const char *last_error;
    int device;
    int volume;
    bool muted;
#if RG_AUDIO_I2S_SAMPLE_RATE
    unsigned sample_rate;
    unsigned phase;
    rg_audio_frame_t previous;
    bool have_previous;
#endif
} state;

static bool driver_init(int device, int sample_rate)
{
    state.last_error = NULL;
    state.device = device;
    if (sample_rate <= 0)
    {
        state.last_error = "Invalid sample rate";
        return false;
    }
#if RG_AUDIO_I2S_SAMPLE_RATE
    state.sample_rate = sample_rate;
    state.phase = 0;
    state.have_previous = false;
    sample_rate = RG_AUDIO_I2S_SAMPLE_RATE;
#endif

    #ifdef RG_GPIO_SND_AMP_ENABLE
        gpio_reset_pin(RG_GPIO_SND_AMP_ENABLE);
        gpio_set_level(RG_GPIO_SND_AMP_ENABLE, MUTE_ENABLE);
        gpio_set_direction(RG_GPIO_SND_AMP_ENABLE, GPIO_MODE_OUTPUT);
    #endif

    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_MSB,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
        }, 0, NULL);
        if (ret == ESP_OK)
            ret = i2s_set_dac_mode(RG_AUDIO_USE_INT_DAC);
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
    #else
        state.last_error = "This device does not support internal DAC mode!";
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_EXT_DAC
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
#if RG_AUDIO_I2S_SAMPLE_RATE
            .tx_desc_auto_clear = true, // Output silence instead of replaying old samples on underrun.
#endif
        #if CONFIG_IDF_TARGET_ESP32
            .use_apll = true, // External DAC may care about accuracy
        #endif
        }, 0, NULL);
        if (ret == ESP_OK)
        {
            ret = i2s_set_pin(I2S_NUM_0, &(i2s_pin_config_t) {
                .mck_io_num = GPIO_NUM_NC,
                .bck_io_num = RG_GPIO_SND_I2S_BCK,
                .ws_io_num = RG_GPIO_SND_I2S_WS,
                .data_out_num = RG_GPIO_SND_I2S_DATA,
                .data_in_num = GPIO_NUM_NC
            });
            if (ret != ESP_OK)
                i2s_driver_uninstall(I2S_NUM_0);
        }
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
    #else
        state.last_error = "This device does not support external DAC mode!";
    #endif
    }
    return state.last_error == NULL;
}

static bool driver_set_sample_rates(int sampleRate)
{
#if RG_AUDIO_I2S_SAMPLE_RATE
    if (sampleRate <= 0)
        return false;
    state.sample_rate = sampleRate;
    state.phase = 0;
    state.have_previous = false;
    return true;
#else
    return i2s_set_sample_rates(I2S_NUM_0, sampleRate) == ESP_OK;
#endif
}

static bool driver_deinit(void)
{
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_set_level(RG_GPIO_SND_AMP_ENABLE, MUTE_ENABLE);
    #endif
    i2s_driver_uninstall(I2S_NUM_0);
    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_EXT_DAC
        gpio_reset_pin(RG_GPIO_SND_I2S_BCK);
        gpio_reset_pin(RG_GPIO_SND_I2S_DATA);
        gpio_reset_pin(RG_GPIO_SND_I2S_WS);
    #endif
    }
    // Keep the amplifier shut down until the next initialization.
    return true;
}

static bool driver_submit(const rg_audio_frame_t *frames, size_t count)
{
    float volume = state.muted ? 0.f : (state.volume * 0.01f);
    bool use_internal_dac = state.device == 0;
    rg_audio_frame_t buffer[DMA_BUFFER_LEN];
    size_t pos = 0;

    for (size_t i = 0; i < count; ++i)
    {
        int left = frames[i].left * volume;
        int right = frames[i].right * volume;

        #if RG_AUDIO_EXT_DAC_MONO
        // SD_MODE high selects one channel on MAX98357A. Feed the mono mix to both slots.
        if (!use_internal_dac)
            left = right = (left + right) / 2;
        #endif

        if (use_internal_dac)
        {
        #if RG_AUDIO_USE_INT_DAC == 1
            left = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
            right = 0;
        #elif RG_AUDIO_USE_INT_DAC == 2
            left = 0;
            right = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
        #elif RG_AUDIO_USE_INT_DAC == 3
            // In two channel mode we use left and right as a differential mono output to increase resolution.
            int sample = (left + right) >> 1;
            if (sample > 0x7F00)
            {
                left = 0x8000 + (sample - 0x7F00);
                right = -0x8000 + 0x7F00;
            }
            else if (sample < -0x7F00)
            {
                left = 0x8000 + (sample + 0x7F00);
                right = -0x8000 + -0x7F00;
            }
            else
            {
                left = 0x8000;
                right = -0x8000 + sample;
            }
        #endif
        }

        // Clipping   (not necessary, we have (int16 * vol) and volume is never more than 1.0)
        // if (left > 32767) left = 32767; else if (left < -32768) left = -32767;
        // if (right > 32767) right = 32767; else if (right < -32768) right = -32767;

#if RG_AUDIO_I2S_SAMPLE_RATE
        // Carry the fractional position and previous frame across submissions.
        if (!state.have_previous)
        {
            state.previous = (rg_audio_frame_t){.left = left, .right = right};
            state.have_previous = true;
        }
        state.phase += RG_AUDIO_I2S_SAMPLE_RATE;
        while (state.phase >= state.sample_rate)
        {
            state.phase -= state.sample_rate;
            buffer[pos].left = left + (int64_t)(state.previous.left - left) * state.phase / RG_AUDIO_I2S_SAMPLE_RATE;
            buffer[pos].right = right + (int64_t)(state.previous.right - right) * state.phase / RG_AUDIO_I2S_SAMPLE_RATE;
#else
        {
            buffer[pos].left = left;
            buffer[pos].right = right;
#endif
            if (++pos != RG_COUNT(buffer))
                continue;
            size_t written = 0;
            size_t bytes = pos * sizeof(buffer[0]);
            if (i2s_write(I2S_NUM_0, (void *)buffer, bytes, &written, 1000) != ESP_OK || written != bytes)
            {
                RG_LOGW("I2S Submission error! Written: %u/%u\n", (unsigned)written, (unsigned)bytes);
                return false;
            }
            pos = 0;
        }
#if RG_AUDIO_I2S_SAMPLE_RATE
        state.previous = (rg_audio_frame_t){.left = left, .right = right};
#endif
    }
    if (pos)
    {
        size_t written = 0;
        size_t bytes = pos * sizeof(buffer[0]);
        if (i2s_write(I2S_NUM_0, buffer, bytes, &written, 1000) != ESP_OK || written != bytes)
        {
            RG_LOGW("I2S Submission error! Written: %u/%u\n", (unsigned)written, (unsigned)bytes);
            return false;
        }
    }
    return true;
}

static bool driver_set_mute(bool mute)
{
    i2s_zero_dma_buffer(I2S_NUM_0);
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_set_level(RG_GPIO_SND_AMP_ENABLE, mute ? MUTE_ENABLE : MUTE_DISABLE);
    #endif
    state.muted = mute;
#if RG_AUDIO_I2S_SAMPLE_RATE
    state.have_previous = false;
#endif
    return true;
}

static bool driver_set_volume(int volume)
{
    state.volume = volume;
    return true;
}

static const char *driver_get_error(void)
{
    return state.last_error;
}

const rg_audio_driver_t rg_audio_driver_i2s = {
    .name = "i2s",
    .init = driver_init,
    .deinit = driver_deinit,
    .submit = driver_submit,
    .set_mute = driver_set_mute,
    .set_volume = driver_set_volume,
    .set_sample_rate = driver_set_sample_rates,
    .get_error = driver_get_error,
};

#endif // RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC
