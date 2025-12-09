#include "sb16.h"
#include "io.h"
#include <stddef.h>

// 全局设备实例
sb16_device_t sb16 = {0};

// DMA缓冲区（必须低于16MB，因为ISA DMA只能访问低16MB内存）
static uint8_t dma_buffer[65536] __attribute__((aligned(4096)));

// 内部函数声明
static void dsp_write(uint8_t value);
static uint8_t dsp_read(void);
static bool dsp_reset(void);
static void mixer_write(uint8_t reg, uint8_t value);
static uint8_t mixer_read(uint8_t reg);
static void dma_init(uint8_t channel, uint8_t* buffer, uint32_t size);
static void dma_start(uint8_t channel);
static void dma_stop(uint8_t channel);
static void set_sample_rate(uint32_t rate);
static void irq_install(void);
static void irq_wait(void);

// 简单的正弦函数近似（避免使用数学库）
static double simple_sin(double x) {
    // 使用泰勒级数近似：sin(x) ≈ x - x³/6 + x⁵/120
    double x2 = x * x;
    double x3 = x * x2;
    double x5 = x3 * x2;
    return x - x3 / 6.0 + x5 / 120.0;
}

// 写入DSP
static void dsp_write(uint8_t value) {
    while ((inb(SB16_DSP_WRITE_STATUS) & 0x80));
    outb(SB16_DSP_WRITE_DATA, value);
}

// 读取DSP
static uint8_t dsp_read(void) {
    while (!(inb(SB16_DSP_READ_STATUS) & 0x80));
    return inb(SB16_DSP_READ_DATA);
}

// 重置DSP
static bool dsp_reset(void) {
    // 发送重置命令
    outb(SB16_DSP_RESET, 1);
    for (volatile int i = 0; i < 100; i++); // 短暂延迟
    
    outb(SB16_DSP_RESET, 0);
    for (volatile int i = 0; i < 100; i++);
    
    // 读取重置响应
    uint8_t response = dsp_read();
    if (response == 0xAA) {
        return true;
    }
    return false;
}

// 写入混音器寄存器
static void mixer_write(uint8_t reg, uint8_t value) {
    outb(SB16_MIXER_ADDR, reg);
    outb(SB16_MIXER_DATA, value);
}

// 读取混音器寄存器
static uint8_t mixer_read(uint8_t reg) {
    outb(SB16_MIXER_ADDR, reg);
    return inb(SB16_MIXER_DATA);
}

// 初始化DMA通道
static void dma_init(uint8_t channel, uint8_t* buffer, uint32_t size) {
    if (channel > 7) return;
    
    // 计算物理地址（假设是identity mapping）
    uintptr_t phys_addr = (uintptr_t)buffer;
    uint32_t page = (phys_addr >> 16) & 0xFF;
    uint32_t offset = phys_addr & 0xFFFF;
    uint32_t count = size - 1;  // DMA计数器是字节数-1
    
    // 屏蔽DMA通道
    outb(DMA_MASK_REG, 0x04 | channel);
    
    // 清除字节指针触发器
    outb(DMA_CLEAR_FF_REG, 0);
    
    // 设置DMA模式：单次传输，地址递增，自动初始化关闭，读传输
    uint8_t mode = 0x48;  // 单次传输模式
    mode |= (channel & 3);
    outb(DMA_MODE_REG, mode);
    
    // 设置地址（低字节->高字节）
    outb(DMA_ADDR_REG(channel), offset & 0xFF);
    outb(DMA_ADDR_REG(channel), (offset >> 8) & 0xFF);
    
    // 设置计数（低字节->高字节）
    outb(DMA_COUNT_REG(channel), count & 0xFF);
    outb(DMA_COUNT_REG(channel), (count >> 8) & 0xFF);
    
    // 设置页面寄存器
    outb(DMA_PAGE_REG(channel), page);
    
    // 取消屏蔽DMA通道
    outb(DMA_MASK_REG, channel & 3);
}

// 启动DMA传输
static void dma_start(uint8_t channel) {
    // 对于8位DMA，我们使用通道1, 5或7
    // 这里我们只启用通道1作为示例
    if (channel == 1) {
        outb(DMA_MASK_REG, 0x01);  // 取消屏蔽通道1
    }
}

// 停止DMA传输
static void dma_stop(uint8_t channel) {
    outb(DMA_MASK_REG, 0x04 | channel);
}

// 设置采样率
static void set_sample_rate(uint32_t rate) {
    if (rate < 4000) rate = 4000;
    if (rate > 44100) rate = 44100;
    
    sb16.sample_rate = rate;
    
    // 发送设置采样率命令
    dsp_write(DSP_CMD_SET_OUTPUT_RATE);
    dsp_write((rate >> 8) & 0xFF);  // 高字节
    dsp_write(rate & 0xFF);         // 低字节
}

// 安装IRQ处理程序（简化版，实际需要设置IDT）
static void irq_install(void) {
    // 注意：在实际内核中，你需要设置中断描述符表(IDT)
    // 并注册sb16_irq_handler作为IRQ5的中断处理程序
    // 这里只是一个占位符实现
}

// 等待IRQ（轮询方式，简化实现）
static void irq_wait(void) {
    // 在实际实现中，这里应该使用中断等待
    // 但为简化，我们使用忙等待
    for (volatile int i = 0; i < 1000000 && !sb16.irq_received; i++);
    sb16.irq_received = false;
}

// 检测SB16声卡
bool sb16_detect(void) {
    // 尝试重置DSP
    if (!dsp_reset()) {
        return false;
    }
    
    // 检查混音器是否存在
    mixer_write(MIXER_MASTER_VOL, 0);
    (void)mixer_read(MIXER_MASTER_VOL); // 读取但不使用
    mixer_write(MIXER_MASTER_VOL, 0xFF);
    if (mixer_read(MIXER_MASTER_VOL) == 0xFF) {
        sb16.detected = true;
        sb16.base_port = 0x220;  // 默认端口
        sb16.irq = SB16_IRQ;
        sb16.dma_channel = DMA_CHANNEL_1;
        return true;
    }
    
    return false;
}

// 初始化SB16
bool sb16_init(void) {
    if (!sb16.detected) {
        if (!sb16_detect()) {
            return false;
        }
    }
    
    // 重置DSP
    if (!dsp_reset()) {
        return false;
    }
    
    // 初始化混音器
    mixer_write(MIXER_MASTER_VOL, 0x3F);  // 中等音量
    mixer_write(MIXER_DAC_VOL, 0x3F);     // PCM音量
    mixer_write(MIXER_SPEAKER, 0x01);     // 启用扬声器
    
    // 设置默认采样率
    set_sample_rate(22050);
    
    // 启用扬声器
    sb16_speaker_on();
    
    // 安装IRQ处理程序（简化）
    irq_install();
    
    // 初始化音频缓冲区
    sb16.audio_buffer = dma_buffer;
    sb16.buffer_size = sizeof(dma_buffer);
    
    return true;
}

// 设置采样率
void sb16_set_sample_rate(uint32_t rate) {
    set_sample_rate(rate);
}

// 设置音量
void sb16_set_volume(uint8_t master, uint8_t pcm) {
    if (master > 0x3F) master = 0x3F;
    if (pcm > 0x3F) pcm = 0x3F;
    
    mixer_write(MIXER_MASTER_VOL, master);
    mixer_write(MIXER_DAC_VOL, pcm);
}

// 播放WAV文件
bool sb16_play_wav(const uint8_t* wav_data, uint32_t size) {
    if (!sb16.detected || size < sizeof(wav_header_t)) {
        return false;
    }
    
    const wav_header_t* header = (const wav_header_t*)wav_data;
    
    // 验证WAV文件格式
    if (header->chunk_id[0] != 'R' || header->chunk_id[1] != 'I' ||
        header->chunk_id[2] != 'F' || header->chunk_id[3] != 'F') {
        return false;
    }
    
    if (header->audio_format != 1) {  // 必须是PCM格式
        return false;
    }
    
    if (header->bits_per_sample != 8 && header->bits_per_sample != 16) {
        return false;  // 只支持8位或16位PCM
    }
    
    // 设置采样率
    set_sample_rate(header->sample_rate);
    
    // 获取音频数据
    const uint8_t* audio_data = wav_data + sizeof(wav_header_t);
    uint32_t audio_size = header->subchunk2_size;
    
    // 播放PCM数据
    return sb16_play_pcm(audio_data, audio_size, header->sample_rate);
}

// 播放原始PCM数据
bool sb16_play_pcm(const uint8_t* pcm_data, uint32_t size, uint32_t sample_rate) {
    if (!sb16.detected || !pcm_data || size == 0) {
        return false;
    }
    
    // 设置采样率
    set_sample_rate(sample_rate);
    
    // 停止当前播放
    sb16_stop();
    
    // 初始化DMA
    uint32_t transfer_size = size;
    if (transfer_size > sb16.buffer_size) {
        transfer_size = sb16.buffer_size;
    }
    
    // 复制数据到DMA缓冲区
    for (uint32_t i = 0; i < transfer_size; i++) {
        sb16.audio_buffer[i] = pcm_data[i];
    }
    
    sb16.buffer_pos = 0;
    sb16.playing = true;
    
    // 设置DMA
    dma_init(sb16.dma_channel, sb16.audio_buffer, transfer_size);
    
    // 设置DSP传输模式（8位PCM）
    dsp_write(DSP_CMD_DMA_8BIT);
    dsp_write((transfer_size - 1) & 0xFF);      // 低字节
    dsp_write(((transfer_size - 1) >> 8) & 0xFF); // 高字节
    
    // 启动DMA
    dma_start(sb16.dma_channel);
    
    return true;
}

// 暂停播放
void sb16_pause(void) {
    if (sb16.playing) {
        dsp_write(DSP_CMD_PAUSE);
        sb16.playing = false;
    }
}

// 恢复播放
void sb16_resume(void) {
    if (!sb16.playing) {
        dsp_write(DSP_CMD_CONTINUE);
        sb16.playing = true;
    }
}

// 停止播放
void sb16_stop(void) {
    if (sb16.playing) {
        dsp_write(DSP_CMD_STOP);
        dma_stop(sb16.dma_channel);
        sb16.playing = false;
    }
}

// 打开扬声器
void sb16_speaker_on(void) {
    dsp_write(DSP_CMD_SPEAKER_ON);
    sb16.speaker_enabled = true;
}

// 关闭扬声器
void sb16_speaker_off(void) {
    dsp_write(DSP_CMD_SPEAKER_OFF);
    sb16.speaker_enabled = false;
}

// 测试声音（生成一个简单的音调）
void sb16_test_sound(void) {
    if (!sb16.detected) return;
    
    // 生成1kHz的方波（0.5秒）
    const uint32_t sample_rate = 22050;
    const uint32_t duration = 1;  // 秒
    const uint32_t num_samples = sample_rate * duration;
    
    uint8_t* test_buffer = sb16.audio_buffer;
    
    // 生成方波（交替高和低）
    for (uint32_t i = 0; i < num_samples; i++) {
        // 每10个样本翻转一次（约1kHz）
        if ((i / 10) % 2 == 0) {
            test_buffer[i] = 200;  // 高电平
        } else {
            test_buffer[i] = 50;   // 低电平
        }
    }
    
    // 播放测试音
    sb16_play_pcm(test_buffer, num_samples, sample_rate);
}

// IRQ处理程序
void sb16_irq_handler(void) {
    // 读取DSP来确认中断
    (void)dsp_read(); // 读取但不使用
    sb16.irq_received = true;
    
    // 通知PIC中断已处理
    outb(0x20, 0x20);  // 发送EOI到PIC
}