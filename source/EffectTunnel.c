/**
 * LASERTUNNEL
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "Tools.h"
#include "Perlin.h"

#include "ModelCorrA.h"
#include "ModelCorrAStaticVerts.h"

#include "ModelCorrB.h"
#include "ModelCorrBStaticVerts.h"

// Global data: Matrices
static int uLocProjection;
static int uLocModelview;
static C3D_Mtx projection;

// Global data: Lighting
static C3D_LightEnv lightEnv;
static C3D_Light light;
static C3D_LightLut lutPhong;
static C3D_LightLut lutShittyFresnel;

static const C3D_Material lightMaterial = {
    { 0.1f, 0.1f, 0.1f }, //ambient
    { 1.0,  1.0,  1.0 }, //diffuse
    { 0.0f, 0.0f, 0.0f }, //specular0
    { 0.0f, 0.0f, 0.0f }, //specular1
    { 0.0f, 0.0f, 0.0f }, //emission
};

// Global data: Zpos sync
const struct sync_track* syncZ;

// Boooones
static int uLocBone[21];

// Other uniforms
static int uLocTexoff;

// Segment struct
typedef struct segment segment;
typedef void (*load_fun_t)(segment* self);
typedef void (*update_fun_t)(segment* self, float row);
typedef void (*draw_fun_t)(segment* self, float row, C3D_Mtx baseView);
typedef void (*delete_fun_t)(segment* self);

struct segment {
    /*
     * filled by genSegments function
     */
    // Functions: Load, update, draw and unload
    load_fun_t load;
    update_fun_t update;
    draw_fun_t draw;
    delete_fun_t delete;
    
    // Segment size
    float length;

    // Alias to other segment
    int32_t alias;

    // Load tracking
    bool loaded;
    bool markLoad;

    /*
     * Filled by load functions
     */
    // Vertices
    vertex_rigged* vbo;
    int32_t vertCount;

    // Slots for two textures
    C3D_Tex texALinear;
    C3D_Tex texBLinear;
    C3D_Tex texCLinear;
    C3D_Tex texDLinear;
    C3D_Tex texELinear;
    C3D_Tex texA;
    C3D_Tex texB;
    C3D_Tex texC;

    // {'Tunnel': 0, 'AdSmall': 1, 'AdBig': 2, 'Engine.002': 3, 'Carriage.002': 4, 'Carriage.001': 5, 'Engine.001': 6, 'Door.Right': 7, 'Door.Left': 8, 'AdSmall.001': 9, 'AdSmall.002': 10, 'AdSmall.003': 11}

    // Slots for "frame" track and two other tracks
    const struct sync_track* syncA;
    const struct sync_track* syncB;
    const struct sync_track* syncC;
    const struct sync_track* syncD;
    const struct sync_track* syncE;
    const struct sync_track* syncF;
    const struct sync_track* syncG;
    const struct sync_track* syncH;
    const struct sync_track* syncI;
    const struct sync_track* syncLight;
    const struct sync_track* syncTexOff;
    const struct sync_track* syncTexSel; 

    // Slot for misc data
    void* other;
};

// Texture loading: Load into linear memory
void loadTexCache(C3D_Tex* tex, C3D_TexCube* cube, const char* path) {
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
void texToVRAM(C3D_Tex* linear, C3D_Tex* vram) {
    if(C3D_TexInitVRAM(vram, linear->width, linear->height, linear->fmt)) {
        C3D_TexLoadImage(vram, linear->data, GPU_TEXFACE_2D, 0);
    } 
    else {
        printf("Texture upload failed!");
    }
}
 
// The actual segments
#define SEGMENT_COUNT 32
segment tunnel[SEGMENT_COUNT];

// Tunnel segment 1: Doors
bool isTexAInVram = false;
int texBStatus = -1;
void loadSegmentDoors(segment* self) {
    // Load vertices
    printf("allocing %d * %d = %d\n", sizeof(vertex_rigged), corrANumVerts, corrANumVerts * sizeof(vertex_rigged));
    printf("Free linear memory after vert alloc: %d\n", linearSpaceFree());

    self->vbo = (vertex_rigged*)linearAlloc(sizeof(vertex_rigged) * corrANumVerts);
    memcpy(&self->vbo[0], corrAVerts, corrANumVerts * sizeof(vertex_rigged));
    self->vertCount = corrANumVerts;

    // Load textures
    isTexAInVram = true;
    texBStatus = 0;
    texToVRAM(&self->texALinear, &self->texA);
    texToVRAM(&self->texCLinear, &self->texB);
    
    C3D_TexSetFilter(&self->texA, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texA, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    C3D_TexSetFilter(&self->texB, GPU_LINEAR, GPU_LINEAR);

    // Set loaded
    self->loaded = true;
}

void updateSegmentDoors(segment* self, float row) {
    // Nada
}

void setBonesFromSync(const struct sync_track* track, float row, int boneFirst, int boneLast) {
    float animPosFloat = sync_get_val(track, row);
    int animPos = (int)animPosFloat;
    float animPosRemainder = animPosFloat - (float)animPos;
    animPos = max(0, min(animPos, 17));
    int animPosNext = (animPos + 1) % 17;

    C3D_Mtx boneMat;   
    for(int i = boneFirst; i <= boneLast; i++) {
        Mtx_Identity(&boneMat);
        for(int j = 0; j < 4 * 3; j++) {
            boneMat.m[j] = corrAAnim[animPos][i][j] * (1.0 - animPosRemainder) + corrAAnim[animPosNext][i][j] * animPosRemainder;
        }
        C3D_FVUnifMtx3x4(GPU_VERTEX_SHADER, uLocBone[i], &boneMat);
    }
}

void drawSegmentDoors(segment* self, float row, C3D_Mtx baseView) {
    waitForA("draw doors");

    // Add VBO to draw buffer
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, (void*)self->vbo, sizeof(vertex_rigged), 4, 0x3210);
    
    waitForA("load bones doors");

    // Get frame and push bones
    setBonesFromSync(self->syncB, row, 0, 1);
    setBonesFromSync(self->syncC, row, 2, 3);
    setBonesFromSync(self->syncD, row, 4, 4);
    setBonesFromSync(self->syncE, row, 5, 5);
    setBonesFromSync(self->syncF, row, 6, 13);
    setBonesFromSync(self->syncG, row, 14, 15);
    setBonesFromSync(self->syncH, row, 16, 16);
    setBonesFromSync(self->syncI, row, 17, 17);

    // Set texcoord offset
    float texoff = sync_get_val(self->syncTexOff, row);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLocTexoff, texoff, 0.0, 0.0, 0.0);

    // Send new modelview
    float rotation = sync_get_val(self->syncA, row);
    Mtx_Translate(&baseView, 0.0, 0.0, -self->length + 18.0, true);
    Mtx_RotateZ(&baseView, M_PI * rotation, true);
    Mtx_RotateY(&baseView, M_PI, true);
    Mtx_RotateX(&baseView, M_PI * 0.5, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &baseView);

    waitForA("tex bind doors");

    // Bind textures
    int texsel = sync_get_val(self->syncTexSel, row);
    if(texsel == 0) {
        if(isTexAInVram == false) {
            waitForA("del/reload to A");
            C3D_TexDelete(&self->texA);
            texToVRAM(&self->texALinear, &self->texA);
            isTexAInVram = true;
        }
        waitForA("tex bind doors 1");
        C3D_TexBind(0, &self->texA);
        waitForA("tex bind doors 1x");
        if(texBStatus != 1) {
            C3D_TexDelete(&self->texB);
            texToVRAM(&self->texDLinear, &self->texB);
            C3D_TexSetWrap(&self->texB, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            texBStatus = 1;
        }
        C3D_TexBind(1, &self->texB);
    }
    if(texsel == 1) {
        if(isTexAInVram == false) {
            waitForA("del/reload to A");
            C3D_TexDelete(&self->texA);
            texToVRAM(&self->texALinear, &self->texA);
            isTexAInVram = true;
        }
        waitForA("tex bind doors 1");
        C3D_TexBind(0, &self->texA);
        waitForA("tex bind doors 1x");
        if(texBStatus != 2) {
            C3D_TexDelete(&self->texB);
            texToVRAM(&self->texELinear, &self->texB);
            C3D_TexSetWrap(&self->texB, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            texBStatus = 2;
        }
        C3D_TexBind(1, &self->texB);
    }
    if(texsel == 2) {
        if(isTexAInVram == false) {
            waitForA("del/reload to A");
            C3D_TexDelete(&self->texA);
            texToVRAM(&self->texALinear, &self->texA);
            isTexAInVram = true;
        }
        if(texBStatus != 0) {
            C3D_TexDelete(&self->texB);
            texToVRAM(&self->texCLinear, &self->texB);
            C3D_TexSetWrap(&self->texB, GPU_REPEAT, GPU_REPEAT);    
            texBStatus = 0;
        }
        waitForA("tex bind doors 2");
        C3D_TexBind(0, &self->texA);
        C3D_TexBind(1, &self->texB);
    }
    if(texsel == 3) {
        if(isTexAInVram == true) {
            waitForA("del/reload to B");
            C3D_TexDelete(&self->texA);
            texToVRAM(&self->texBLinear, &self->texA);
            isTexAInVram = false;
        }
        waitForA("tex bind doors 3");
        C3D_TexBind(0, &self->texA);
        if(texBStatus != 2) {
            C3D_TexDelete(&self->texB);
            texToVRAM(&self->texELinear, &self->texB);
            C3D_TexSetWrap(&self->texB, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            texBStatus = 2;
        }
        C3D_TexBind(1, &self->texB);
    }

    waitForA("lightenv doors");

    // Set up lightenv
    C3D_LightEnvInit(&lightEnv);
    C3D_LightEnvBind(&lightEnv);
    
    LightLut_Phong(&lutPhong, 100.0);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lutPhong);
    
    LightLut_FromFunc(&lutShittyFresnel, badFresnel, 1.9, false);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_FR, GPU_LUTINPUT_NV, false, &lutShittyFresnel);
    C3D_LightEnvFresnel(&lightEnv, GPU_PRI_SEC_ALPHA_FRESNEL);
    
    C3D_FVec lightVec = FVec4_New(0.0, 0.0, -4.0, 1.0);
    C3D_LightInit(&light, &lightEnv);

    float lightStrength = sync_get_val(self->syncLight, row);
    C3D_LightColor(&light, lightStrength, lightStrength, lightStrength);
    C3D_LightPosition(&light, &lightVec);
    C3D_LightEnvMaterial(&lightEnv, &lightMaterial);

    // Set up draw env
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_FRAGMENT_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
    
    env = C3D_GetTexEnv(1);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE1, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);

    // GPU state for normal drawing
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    waitForA("drawcall doors");

    // Now draw
    C3D_DrawArrays(GPU_TRIANGLES, 0, self->vertCount);
}

void deleteSegmentDoors(segment* self) {
    self->loaded = false;
    linearFree(self->vbo);
    C3D_TexDelete(&self->texA);
}

void genSegmentDoors(segment* self, char* syncPrefix) {
    self->load = loadSegmentDoors;
    self->update = updateSegmentDoors;
    self->draw = drawSegmentDoors;
    self->delete = deleteSegmentDoors;
    self->length = 157.0;
    self->alias = -1;

    // Get sync params
    char paramName[255];
    sprintf(paramName, "%s.rot", syncPrefix);
    self->syncA = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.door", syncPrefix);
    self->syncB = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.lamp", syncPrefix);
    self->syncC = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.cat", syncPrefix);
    self->syncD = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.head", syncPrefix);
    self->syncE = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.chair", syncPrefix);
    self->syncF = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.tables", syncPrefix);
    self->syncG = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.zeppe", syncPrefix);
    self->syncH = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.frame", syncPrefix);
    self->syncI = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.light", syncPrefix);
    self->syncLight = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.texoff", syncPrefix);
    self->syncTexOff = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.texsel", syncPrefix);
    self->syncTexSel = sync_get_track(rocket, paramName);

    self->loaded = false;
}

// Tunnel segment 2: Train tunnel
void loadSegmentTrain(segment* self) {
    // Load vertices
    printf("allocing %d * %d = %d\n", sizeof(vertex_rigged), corrBNumVerts, corrBNumVerts * sizeof(vertex_rigged));
    printf("Free linear memory after vert alloc: %d\n", linearSpaceFree());

    self->vbo = (vertex_rigged*)linearAlloc(sizeof(vertex_rigged) * corrBNumVerts);
    memcpy(&self->vbo[0], corrBVerts, corrBNumVerts * sizeof(vertex_rigged));
    self->vertCount = corrBNumVerts;

    // Load textures
    texToVRAM(&self->texALinear, &self->texA);
    C3D_TexSetFilter(&self->texA, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texA, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    texToVRAM(&self->texBLinear, &self->texB);
    C3D_TexSetFilter(&self->texB, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texB, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    texToVRAM(&self->texCLinear, &self->texC);
    C3D_TexSetFilter(&self->texC, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texC, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    // Set loaded
    self->loaded = true;
}

void updateSegmentTrain(segment* self, float row) {
    // Nada
}

void setBonesFromSyncB(const struct sync_track* track, float row, int boneFirst, int boneLast) {
    float animPosFloat = sync_get_val(track, row);
    int animPos = (int)animPosFloat;
    float animPosRemainder = animPosFloat - (float)animPos;
    animPos = max(0, min(animPos, 17));
    int animPosNext = (animPos + 1) % 17;

    C3D_Mtx boneMat;   
    for(int i = boneFirst; i <= boneLast; i++) {
        Mtx_Identity(&boneMat);
        for(int j = 0; j < 4 * 3; j++) {
            boneMat.m[j] = corrBAnim[animPos][i][j] * (1.0 - animPosRemainder) + corrBAnim[animPosNext][i][j] * animPosRemainder;
        }
        C3D_FVUnifMtx3x4(GPU_VERTEX_SHADER, uLocBone[i], &boneMat);
    }
}

void drawSegmentTrain(segment* self, float row, C3D_Mtx baseView) {
    waitForA("draw train");

    // Add VBO to draw buffer
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, (void*)self->vbo, sizeof(vertex_rigged), 4, 0x3210);
        
    // Get frame and push bones
    setBonesFromSyncB(self->syncB, row, 0, 2);
    setBonesFromSyncB(self->syncB, row, 9, 11);
    setBonesFromSyncB(self->syncD, row, 3, 6);
    setBonesFromSyncB(self->syncC, row, 7, 8);

    // Set texcoord offset
    //float texoff = sync_get_val(self->syncTexOff, row);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, uLocTexoff, 0.0, 0.0, 0.0, 0.0);

    // Send new modelview
    float rotation = sync_get_val(self->syncA, row);
    Mtx_Translate(&baseView, 0.0, 0.0, -self->length + 810.0, true);
    Mtx_RotateZ(&baseView, M_PI * rotation, true);
    Mtx_RotateY(&baseView, M_PI, true);
    Mtx_RotateX(&baseView, M_PI * 0.5, true);
    Mtx_Scale(&baseView, 8.0, 8.0, 8.0);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &baseView);

    waitForA("bind");

    // Bind textures
    int texsel = sync_get_val(self->syncTexSel, row);
    if(texsel == 0) {
        C3D_TexBind(1, &self->texA);
        C3D_TexBind(0, &self->texB);
    }
    if(texsel == 1) {
        C3D_TexBind(1, &self->texA);
        C3D_TexBind(0, &self->texC);
    }

    waitForA("bound");

    // Set up lightenv
    C3D_LightEnvInit(&lightEnv);
    C3D_LightEnvBind(&lightEnv);
    
    LightLut_Phong(&lutPhong, 100.0);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lutPhong);
    
    LightLut_FromFunc(&lutShittyFresnel, badFresnel, 1.9, false);
    C3D_LightEnvLut(&lightEnv, GPU_LUT_FR, GPU_LUTINPUT_NV, false, &lutShittyFresnel);
    C3D_LightEnvFresnel(&lightEnv, GPU_PRI_SEC_ALPHA_FRESNEL);
    
    waitForA("draw");

    C3D_FVec lightVec = FVec4_New(0.0, 0.0, -4.0, 1.0);
    C3D_LightInit(&light, &lightEnv);

    float lightStrength = sync_get_val(self->syncLight, row);
    C3D_LightColor(&light, lightStrength, lightStrength, lightStrength);
    C3D_LightPosition(&light, &lightVec);
    C3D_LightEnvMaterial(&lightEnv, &lightMaterial);

    waitForA("lutset");

    // Set up draw env
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_FRAGMENT_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
    
    env = C3D_GetTexEnv(1);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE1, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);

    // GPU state for normal drawing
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    waitForA("drawcall");

    // Now draw
    C3D_DrawArrays(GPU_TRIANGLES, 0, self->vertCount);
}

void deleteSegmentTrain(segment* self) {
    self->loaded = false;
    linearFree(self->vbo);
    C3D_TexDelete(&self->texA);
}

void genSegmentTrain(segment* self, char* syncPrefix) {
    self->load = loadSegmentTrain;
    self->update = updateSegmentTrain;
    self->draw = drawSegmentTrain;
    self->delete = deleteSegmentTrain;
    self->length = 1154.0;
    self->alias = -1;

    // Get sync params
    char paramName[255];
    sprintf(paramName, "%s.rot", syncPrefix);
    self->syncA = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.ad", syncPrefix);
    self->syncB = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.door", syncPrefix);
    self->syncC = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.train", syncPrefix);
    self->syncD = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.texsel", syncPrefix);
    self->syncTexSel = sync_get_track(rocket, paramName);
    sprintf(paramName, "%s.light", syncPrefix);
    self->syncLight = sync_get_track(rocket, paramName);

    self->loaded = false;
}

void effectTunnelInit() {
    // Prep general info: Shader
    C3D_BindProgram(&shaderProgramBones);

    // Prep general info: Uniform locs
    uLocProjection = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "projection");
    uLocModelview = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "modelView");
    uLocTexoff = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, "texoff");
    
    // Prep general info: Bones 
    char boneName[255];
    for(int i = 0; i < 21; i++) {
        sprintf(boneName, "bone%02d", i);
        uLocBone[i] = shaderInstanceGetUniformLocation(shaderProgramBones.vertexShader, boneName);
    }

    // Prep general info: Z pos sync
    syncZ = sync_get_track(rocket, "global.z");

    // Init tunnel segments
    genSegmentDoors(&tunnel[0], "doors");
    loadTexCache(&tunnel[0].texALinear, NULL, "romfs:/tex_corr_real.bin");
    loadTexCache(&tunnel[0].texBLinear, NULL, "romfs:/tex_corr.bin");
    loadTexCache(&tunnel[0].texCLinear, NULL, "romfs:/tex_disco.bin");
    loadTexCache(&tunnel[0].texDLinear, NULL, "romfs:/tex_logo1.bin");
    loadTexCache(&tunnel[0].texELinear, NULL, "romfs:/tex_logo2.bin");
    tunnel[0].load(&tunnel[0]);
    
    waitForA("gen");
    genSegmentTrain(&tunnel[8], "train");
    waitForA("load");
    loadTexCache(&tunnel[8].texALinear, NULL, "romfs:/tex_tunnel.bin");
    loadTexCache(&tunnel[8].texBLinear, NULL, "romfs:/tex_greets1.bin");
    loadTexCache(&tunnel[8].texCLinear, NULL, "romfs:/tex_greets2.bin");

    waitForA("load B");
    tunnel[8].load(&tunnel[8]);
    waitForA("copy");
    genSegmentTrain(&tunnel[9], "trainOdd");
    tunnel[9].vbo = tunnel[8].vbo;
    tunnel[9].texA = tunnel[8].texA;
    tunnel[9].texB = tunnel[8].texB;
    tunnel[9].texC = tunnel[8].texC;
    tunnel[9].texALinear = tunnel[8].texALinear;
    tunnel[9].texBLinear = tunnel[8].texBLinear;
    tunnel[9].texCLinear = tunnel[8].texCLinear;
    tunnel[9].vertCount = tunnel[8].vertCount;

    for(int i = 1; i < 8; i++) {
        if(i % 2 == 0) {
            genSegmentDoors(&tunnel[i], "doors");
        }
        else {
            genSegmentDoors(&tunnel[i], "doorsOdd");
        }
        tunnel[i].vbo = tunnel[0].vbo;
        tunnel[i].texA = tunnel[0].texA;
        tunnel[i].texB = tunnel[0].texB;
        tunnel[i].texALinear = tunnel[0].texALinear;
        tunnel[i].texBLinear = tunnel[0].texBLinear;
        tunnel[i].texCLinear = tunnel[0].texCLinear;
        tunnel[i].texDLinear = tunnel[0].texDLinear;
        tunnel[i].texELinear = tunnel[0].texELinear;
        tunnel[i].vertCount = tunnel[0].vertCount;
    }
    for(int i = 10; i < SEGMENT_COUNT; i++) {
        if(i % 2 == 0) {
            genSegmentDoors(&tunnel[i], "doors");
        }
        else {
            genSegmentDoors(&tunnel[i], "doorsOdd");
        }
        tunnel[i].vbo = tunnel[0].vbo;
        tunnel[i].texA = tunnel[0].texA;
        tunnel[i].texB = tunnel[0].texB; 
        tunnel[i].texALinear = tunnel[0].texALinear;
        tunnel[i].texBLinear = tunnel[0].texBLinear;
        tunnel[i].texCLinear = tunnel[0].texCLinear;
        tunnel[i].texDLinear = tunnel[0].texDLinear;
        tunnel[i].texELinear = tunnel[0].texELinear;
        tunnel[i].vertCount = tunnel[0].vertCount;
    }
    waitForA("done copy");
}

void tunnelShadeEnv() {
    // Set up attribute info
    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 1); // v1 = bone indices
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3); // v3 = normal        
    AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 4); // v4 = texcoords

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C3D_BindProgram(&shaderProgramBones);
    C3D_CullFace(GPU_CULL_BACK_CCW);
}

void effectTunnelDraw(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
    waitForA("etd called");  

    // Get Z for overall effect
    float tunnelZ = sync_get_val(syncZ, row);

    waitForA("mark load");  

    // Reset load marker for everything
    for(int i = 0; i < SEGMENT_COUNT; i++) {
        tunnel[i].markLoad = false;
    }

    // If behind cam, unload
    float tunnelLoadPos = -tunnelZ;
    for(int i = 0; i < SEGMENT_COUNT; i++) {
        tunnelLoadPos += tunnel[i].length;
        if(tunnelLoadPos < -16.0) {
            tunnel[i].markLoad = false;
        }
    }

    // Set active segments
    tunnelLoadPos = -tunnelZ;
    int loaded = 0;
    for(int i = 0; i < SEGMENT_COUNT; i++) {
        tunnelLoadPos += tunnel[i].length;

        // Load if in front of cam and less than 2 loaded
        if(tunnelLoadPos > -16.0 && loaded < 2) {
            tunnel[i].markLoad = true;
            loaded++;
        }
    }

    waitForA("before drawcalls");        

    // Left eye
    tunnelShadeEnv();
    C3D_Mtx modelview;
    Mtx_Identity(&modelview);
    Mtx_Translate(&modelview, 0.0, 0.0, tunnelZ, true);

    C3D_FrameDrawOn(targetLeft);
    C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0);
    Mtx_PerspStereoTilt(&projection, 90.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, -iod, 2.0f, false);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);

    // Dispatch drawcalls
    for(int i = 0; i < SEGMENT_COUNT; i++) {
        if(tunnel[i].markLoad == true) {
            waitForA("dispatch");
            tunnel[i].draw(&tunnel[i], row, modelview);
        }
        Mtx_Translate(&modelview, 0.0, 0.0, -tunnel[i].length, true);
    }
    fade();

    // Right eye?
    if(iod > 0.0) {
        tunnelShadeEnv();
        Mtx_Identity(&modelview);
        Mtx_Translate(&modelview, 0.0, 0.0, tunnelZ, true);

        C3D_FrameDrawOn(targetRight);
        C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, 0x000000FF, 0); 

        Mtx_PerspStereoTilt(&projection, 90.0f*M_PI/180.0f, 400.0f/240.0f, 0.01f, 6000.0f, iod, 2.0f, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projection);

        // Dispatch drawcalls
        for(int i = 0; i < SEGMENT_COUNT; i++) {
            if(tunnel[i].markLoad == true) {
                tunnel[i].draw(&tunnel[i], row, modelview);
            }
            Mtx_Translate(&modelview, 0.0, 0.0, -tunnel[i].length, true);
        }
        fade();
    }

    // Swap
    C3D_FrameEnd(0);
}
