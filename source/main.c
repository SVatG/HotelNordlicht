/**
 * Nordlicht 2022 - Hotel Nordlicht
 * 
 * SVatG 2022 ~ halcy / Saga Musix / dotUser
 **/

#include "Tools.h"
#include "Rocket/sync.h"

#define CLEAR_COLOR 0x555555FF

C3D_Tex fade_tex;
static Pixel* fadePixels;
static Bitmap fadeBitmap;
float fadeVal;

u8* music_bin;
u32 music_bin_size;

// u8* music_bin_play;
// u32 music_bin_play_block;
// 
// u8* music_bin_preload;
// u32 music_bin_preload_block;

#define min(a, b) (((a)<(b))?(a):(b))
#define max(a, b) (((a)>(b))?(a):(b))

#define AUDIO_BUFSIZE 512
#define AUDIO_BLOCKSIZE 16384

#define SONG_BPM 129.0
#define SONG_BPS (SONG_BPM / 60.0)
#define SONG_SPS 32000
#define SONG_SPB (SONG_SPS / SONG_BPS)

#define ROWS_PER_BEAT 8
#define SAMPLES_PER_ROW (SONG_SPB / ROWS_PER_BEAT)

int32_t sample_pos = 0;
ndspWaveBuf wave_buffer[2];
uint8_t fill_buffer = 0;
uint8_t audio_playing = 1;

double audio_get_row() {
    return (double)sample_pos / (double)SAMPLES_PER_ROW;
}

// #define DEV_MODE

#ifndef SYNC_PLAYER
void audio_pause(void *ignored, int flag) {
   ignored;
   audio_playing = !flag;
}

void audio_set_row(void *ignored, int row) {
    ignored;
    sample_pos = row * SAMPLES_PER_ROW;
}

int audio_is_playing(void *d) {
    return audio_playing;
}

struct sync_cb rocket_callbakcks = {
    audio_pause,
    audio_set_row,
    audio_is_playing
};
#endif

// #define ROCKET_HOST "192.168.56.1"
// #define ROCKET_HOST "192.168.1.129"
// #define ROCKET_HOST "127.0.0.1"
#define ROCKET_HOST "192.168.10.129"

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

static uint32_t *SOC_buffer = NULL;

int connect_rocket() {
#ifndef SYNC_PLAYER
    while(sync_tcp_connect(rocket, ROCKET_HOST, SYNC_DEFAULT_PORT)) {
        printf("Didn't work, again...\n");
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            return(1);
        }
        svcSleepThread(1000*1000*1000);
    }
#endif
    return(0);
}

extern void effectTunnelInit();
extern void effectTunnelDraw(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod);

FILE *audioFile;
u8 audioTempBuf[AUDIO_BUFSIZE * 4];
void audio_callback(void* ignored) {
    ignored;
    if(wave_buffer[fill_buffer].status == NDSP_WBUF_DONE && (sample_pos + AUDIO_BUFSIZE) * sizeof(int16_t) < music_bin_size) {
        if(audio_playing == 1) {
            sample_pos += AUDIO_BUFSIZE;
        }
        uint8_t *dest = (uint8_t*)wave_buffer[fill_buffer].data_pcm16;
        
        // For audio streaming (WIP)
//         memcpy(dest, audioTempBuf, AUDIO_BUFSIZE * sizeof(int16_t));
//         wantAudioData = 1;
//         int block_id = (sample_pos - AUDIO_BUFSIZE) / AUDIO_BLOCKSIZE;
//         if(block_id != music_bin_play_block) {
//             printf("Copying audio %d != %d @ %d\n", block_id, music_bin_play_block, sample_pos);
//             memcpy(music_bin_play, music_bin_preload, AUDIO_BLOCKSIZE * sizeof(int16_t));
//             music_bin_play_block = block_id;
//         }
//         int play_pos = (sample_pos - AUDIO_BUFSIZE) % AUDIO_BLOCKSIZE;
        
//         memcpy(dest, &music_bin_play[play_pos * sizeof(int16_t)], AUDIO_BUFSIZE * sizeof(int16_t));
        memcpy(dest, &music_bin[(sample_pos - AUDIO_BUFSIZE) * sizeof(int16_t)], AUDIO_BUFSIZE * sizeof(int16_t));        
        DSP_FlushDataCache(dest, AUDIO_BUFSIZE * sizeof(int16_t));
        ndspChnWaveBufAdd(0, &wave_buffer[fill_buffer]);
        fill_buffer = !fill_buffer;
    }
}

#include <vshader_normalmapping_shbin.h>
#include <vshader_skybox_shbin.h>
#include <vshader_shbin.h>
#include <vshader_bones_shbin.h>

DVLB_s* vshader_dvlb;
DVLB_s* vshader_normalmapping_dvlb;
DVLB_s* vshader_bones_dvlb;
DVLB_s* vshader_skybox_dvlb;
shaderProgram_s shaderProgram;
shaderProgram_s shaderProgramNormalMapping;
shaderProgram_s shaderProgramBones;
shaderProgram_s shaderProgramSkybox;

// For audio streaming (WIP)
// void music_preload(int block_id) {
//     if(block_id != music_bin_preload_block) {
//         int load_pos = block_id * AUDIO_BLOCKSIZE;
// //         printf("Preloading block %d != %d @ %d\n", block_id, music_bin_preload_block, load_pos);
//         fseek(audioFile, load_pos * sizeof(int16_t), SEEK_SET);
//         fread(music_bin_preload, AUDIO_BLOCKSIZE * sizeof(int16_t), 1, audioFile);
//         music_bin_preload_block = block_id;
//     }
// }

// Texture loading: Load into linear memory
void loadTexCache2(C3D_Tex* tex, C3D_TexCube* cube, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Texture file not found: %s\n", path);
        return;
    }
    
    Tex3DS_Texture t3x = Tex3DS_TextureImportStdio(f, tex, cube, false);
    fclose(f); 
    if (!t3x) {
        printf("Final texture load failure on %s\n", path);
        return;
    }
    
    // Delete the t3x object since we don't need it
    Tex3DS_TextureFree(t3x);

    printf("Free linear memory after tex load: %d\n", linearSpaceFree());
}

// Texture loading: Linear to VRAM
void texToVRAM2(C3D_Tex* linear, C3D_Tex* vram) {
    if(C3D_TexInitVRAM(vram, linear->width, linear->height, linear->fmt)) {
        C3D_TexLoadImage(vram, linear->data, GPU_TEXFACE_2D, 0);
    } 
    else {
        printf("Texture upload failed!");
    }
}

C3D_Tex scrollImgs[8];
int vramScrollImg = -1;

int main() {
    bool DUMPFRAMES = false;
    bool DUMPFRAMES_3D = false;

    // Initialize graphics
    gfxInit(GSP_RGBA8_OES, GSP_BGR8_OES, false);
    gfxSet3D(true);
    consoleInit(GFX_BOTTOM, NULL);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    // Initialize the render target
    C3D_RenderTarget* targetLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(targetLeft, GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
    C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);
    
    romfsInit();
    
    // Open music
    music_bin = readFileMem("romfs:/music2.bin", &music_bin_size, false);
    
    // Rocket startup
#ifndef SYNC_PLAYER
    printf("Now socketing...\n");
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(SOC_buffer, SOC_BUFFERSIZE);
    
    rocket = sync_create_device("sdmc:/sync");
#else
    rocket = sync_create_device("romfs:/sync");
#endif
    if(connect_rocket()) {
        return(0);
    }
    
    // Load shaders
    vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&shaderProgram);
    shaderProgramSetVsh(&shaderProgram, &vshader_dvlb->DVLE[0]);

    vshader_skybox_dvlb = DVLB_ParseFile((u32*)vshader_skybox_shbin, vshader_skybox_shbin_size);
    shaderProgramInit(&shaderProgramSkybox);
    shaderProgramSetVsh(&shaderProgramSkybox, &vshader_skybox_dvlb->DVLE[0]);

    vshader_bones_dvlb = DVLB_ParseFile((u32*)vshader_bones_shbin, vshader_bones_shbin_size);
    shaderProgramInit(&shaderProgramBones);
    shaderProgramSetVsh(&shaderProgramBones, &vshader_bones_dvlb->DVLE[0]);
    
    vshader_normalmapping_dvlb = DVLB_ParseFile((u32*)vshader_normalmapping_shbin, vshader_normalmapping_shbin_size);
    shaderProgramInit(&shaderProgramNormalMapping);
    shaderProgramSetVsh(&shaderProgramNormalMapping, &vshader_normalmapping_dvlb->DVLE[0]);
    
    // Sound on
    ndspInit();
    
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SONG_SPS);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;    
    ndspChnSetMix(0, mix);

    uint8_t *audio_buffer = (uint8_t*)linearAlloc(AUDIO_BUFSIZE * sizeof(int16_t) * 2);
    memset(wave_buffer,0,sizeof(wave_buffer));
    wave_buffer[0].data_vaddr = &audio_buffer[0];
    wave_buffer[0].nsamples = AUDIO_BUFSIZE;
    wave_buffer[1].data_vaddr = &audio_buffer[AUDIO_BUFSIZE * sizeof(int16_t)];
    wave_buffer[1].nsamples = AUDIO_BUFSIZE;
    
    // Get first row value
    double row = 0.0;  

#ifndef SYNC_PLAYER
    if(sync_update(rocket, (int)floor(row), &rocket_callbakcks, (void *)0)) {
        printf("Lost connection, retrying.\n");
        if(connect_rocket()) {
            return(0);
        }
    }
#endif

    // Load scroll textures
    loadTexCache2(&scrollImgs[0], NULL, "romfs:/tex_scroll1.bin");
    loadTexCache2(&scrollImgs[1], NULL, "romfs:/tex_scroll2.bin");
    loadTexCache2(&scrollImgs[2], NULL, "romfs:/tex_scroll3.bin");
    loadTexCache2(&scrollImgs[3], NULL, "romfs:/tex_scroll4.bin");
    loadTexCache2(&scrollImgs[4], NULL, "romfs:/tex_scroll5.bin");
    loadTexCache2(&scrollImgs[5], NULL, "romfs:/tex_scroll6.bin");
    loadTexCache2(&scrollImgs[6], NULL, "romfs:/tex_scroll7.bin");
    loadTexCache2(&scrollImgs[7], NULL, "romfs:/tex_scroll8.bin");
    texToVRAM2(&scrollImgs[0], &fade_tex);

    // Set up effect code
    effectTunnelInit();

    const struct sync_track* sync_fade = sync_get_track(rocket, "global.fade");
    const struct sync_track* sync_img = sync_get_track(rocket, "global.image");
    int fc = 0;

    // Play music
    ndspSetCallback(&audio_callback, 0);
    ndspChnWaveBufAdd(0, &wave_buffer[0]);
    ndspChnWaveBufAdd(0, &wave_buffer[1]);
    
    row = audio_get_row();

    while (aptMainLoop()) {        
        if(!DUMPFRAMES) {
            row = audio_get_row();
        }
        else {
            printf("Frame dump %d\n", fc);
            row = ((double)fc * (32000.0 / 30.0)) / (double)SAMPLES_PER_ROW;
        }
        
#ifndef SYNC_PLAYER
        if(sync_update(rocket, (int)floor(row), &rocket_callbakcks, (void *)0)) {
            printf("Lost connection, retrying.\n");
            if(connect_rocket()) {
                return(0);
            }
        }
#endif

        //printf("fadey\n");
        // Maybe not needed anymore idk
        fadeVal = sync_get_val(sync_fade, row);
        int whichImg = sync_get_val(sync_img, row);
        whichImg = min(whichImg, 7);
        if(whichImg != vramScrollImg) {
            printf("Attempting load of tex: %d\n", whichImg);
            C3D_TexDelete(&fade_tex);
            texToVRAM2(&scrollImgs[whichImg], &fade_tex);
            vramScrollImg = whichImg;
        }
        C3D_TexSetFilter(&fade_tex, GPU_LINEAR, GPU_LINEAR);

        //printf("ppf\n");
        hidScanInput();
        
        // Respond to user input
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            break; // break in order to return to hbmenu
        }  
        float slider = osGet3DSliderState();
        float iod = slider / 3.0;

        effectTunnelDraw(targetLeft, targetRight, row, iod);

        if(DUMPFRAMES) {
            gspWaitForP3D();
            gspWaitForPPF();
            
            u8* fbl = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
            
            char fname[255];
            if(fc < 1000) {
                sprintf(fname, "3ds/dump1/fb_left_%08d.raw", fc);
            }
            else if(fc < 2000) {
                sprintf(fname, "3ds/dump2/fb_left_%08d.raw", fc);
            }
            else if(fc < 3000) {
                sprintf(fname, "3ds/dump3/fb_left_%08d.raw", fc);
            }
            else if(fc < 4000) {
                sprintf(fname, "3ds/dump4/fb_left_%08d.raw", fc);
            }
            FILE* file = fopen(fname,"w");
            fwrite(fbl, sizeof(int32_t), SCREEN_HEIGHT * SCREEN_WIDTH, file);
            fflush(file);
            fclose(file);
            
            if(DUMPFRAMES_3D) {
                u8* fbr = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
                if(fc < 1000) {
                    sprintf(fname, "3ds/dump1/fb_right_%08d.raw", fc);
                }
                else if(fc < 2000) {
                    sprintf(fname, "3ds/dump2/fb_right_%08d.raw", fc);
                }
                else if(fc < 3000) {
                    sprintf(fname, "3ds/dump3/fb_right_%08d.raw", fc);
                }
                else if(fc < 4000) {
                    sprintf(fname, "3ds/dump4/fb_right_%08d.raw", fc);
                }
                file = fopen(fname,"w");
                fwrite(fbr, sizeof(int32_t), SCREEN_HEIGHT * SCREEN_WIDTH, file);
                fflush(file);
                fclose(file);
            }
        }
        
        fc++;
        //gspWaitForPPF();
        //printf("Got to next loop ---------------- \n");
    }
    
    linearFree(fadePixels);
    
    // Sound off
    ndspExit();
    linearFree(audio_buffer);
    
    // Deinitialize graphics
    socExit();
    C3D_Fini();
    gfxExit();
    
    return 0;
}
