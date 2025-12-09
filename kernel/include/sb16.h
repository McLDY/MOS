#ifndef SB16_H
#define SB16_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// SB16 DSP端口地址
#define SB16_DSP_RESET       0x226
#define SB16_DSP_READ_DATA   0x22A
#define SB16_DSP_WRITE_DATA  0x22C
#define SB16_DSP_READ_STATUS 0x22E
#define SB16_DSP_WRITE_STATUS 0x22C  // 和WRITE_DATA相同

// 混音器端口
#define SB16_MIXER_ADDR      0x224
#define SB16_MIXER_DATA      0x225

// 混音器寄存器
#define MIXER_MASTER_VOL     0x22
#define MIXER_DAC_VOL        0x04
#define MIXER_FM_VOL         0x26
#define MIXER_CD_VOL         0x28
#define MIXER_LINE_VOL       0x2E
#define MIXER_MIC_VOL        0x0A
#define MIXER_SPEAKER        0x0D
#define MIXER_OUTPUT_SELECT  0x0C

// DSP命令
#define DSP_CMD_SET_OUTPUT_RATE  0x41
#define DSP_CMD_SET_BLOCK_SIZE   0x48
#define DSP_CMD_OUTPUT_8BIT      0xC0
#define DSP_CMD_SET_TIME_CONST   0x40
#define DSP_CMD_DMA_8BIT         0x14
#define DSP_CMD_DMA_8BIT_HIGH    0x91
#define DSP_CMD_PAUSE            0xD0
#define DSP_CMD_CONTINUE         0xD4
#define DSP_CMD_STOP             0xD5
#define DSP_CMD_SPEAKER_ON       0xD1
#define DSP_CMD_SPEAKER_OFF      0xD3

// DMA通道（SB16通常使用DMA通道1, 5或7）
#define DMA_CHANNEL_1            1
#define DMA_CHANNEL_5            5
#define DMA_CHANNEL_7            7

// DMA控制器端口
#define DMA_ADDR_REG(channel)    (0x00 + ((channel) & 3) * 2)
#define DMA_COUNT_REG(channel)   (0x01 + ((channel) & 3) * 2)
#define DMA_PAGE_REG(channel)    ((channel) < 4 ? 0x87 + (channel) : 0x8F + ((channel) - 4))
#define DMA_MASK_REG             0x0A
#define DMA_MODE_REG             0x0B
#define DMA_CLEAR_FF_REG         0x0C
#define DMA_MASTER_CLEAR         0x0D

// 中断向量（SB16通常使用IRQ 5或7）
#define SB16_IRQ                 5

// WAV文件头结构
#pragma pack(push, 1)
typedef struct {
    char     chunk_id[4];        // "RIFF"
    uint32_t chunk_size;         // 文件总大小-8
    char     format[4];          // "WAVE"
    
    char     subchunk1_id[4];    // "fmt "
    uint32_t subchunk1_size;     // PCM格式块大小(16)
    uint16_t audio_format;       // 音频格式(1=PCM)
    uint16_t num_channels;       // 声道数
    uint32_t sample_rate;        // 采样率
    uint32_t byte_rate;          // 字节率
    uint16_t block_align;        // 块对齐
    uint16_t bits_per_sample;    // 位深度
    
    char     subchunk2_id[4];    // "data"
    uint32_t subchunk2_size;     // 音频数据大小
} wav_header_t;
#pragma pack(pop)

// SB16设备状态
typedef struct {
    bool detected;               // 设备是否检测到
    uint16_t base_port;          // 基础端口地址
    uint8_t irq;                 // 中断号
    uint8_t dma_channel;         // DMA通道
    uint32_t sample_rate;        // 当前采样率
    bool speaker_enabled;        // 扬声器是否启用
    volatile bool playing;       // 是否正在播放
    volatile bool irq_received;  // 中断是否收到
    uint8_t* audio_buffer;       // 音频缓冲区
    uint32_t buffer_size;        // 缓冲区大小
    uint32_t buffer_pos;         // 当前播放位置
} sb16_device_t;

// 函数声明
bool sb16_detect(void);
bool sb16_init(void);
void sb16_set_sample_rate(uint32_t rate);
void sb16_set_volume(uint8_t master, uint8_t pcm);
bool sb16_play_wav(const uint8_t* wav_data, uint32_t size);
bool sb16_play_pcm(const uint8_t* pcm_data, uint32_t size, uint32_t sample_rate);
void sb16_pause(void);
void sb16_resume(void);
void sb16_stop(void);
void sb16_speaker_on(void);
void sb16_speaker_off(void);
void sb16_test_sound(void);
void sb16_irq_handler(void);

// 外部设备实例
extern sb16_device_t sb16;

#endif // SB16_H