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
    C3D_Tex texA;
    C3D_Tex texB;

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
#define SEGMENT_COUNT 8
segment tunnel[SEGMENT_COUNT];

// Tunnel segment 1: Doors
void loadSegmentDoors(segment* self) {
    // Load vertices
    printf("allocing %d * %d = %d\n", sizeof(vertex_rigged), corrANumVerts, corrANumVerts * sizeof(vertex_rigged));
    printf("Free linear memory after vert alloc: %d\n", linearSpaceFree());

    self->vbo = (vertex_rigged*)linearAlloc(sizeof(vertex_rigged) * corrANumVerts);
    memcpy(&self->vbo[0], corrAVerts, corrANumVerts * sizeof(vertex_rigged));
    self->vertCount = corrANumVerts;

    // Load textures
    texToVRAM(&self->texALinear, &self->texA);
    texToVRAM(&self->texCLinear, &self->texB);
    C3D_TexSetFilter(&self->texA, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texA, GPU_REPEAT, GPU_REPEAT);
    C3D_TexSetFilter(&self->texB, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&self->texB, GPU_REPEAT, GPU_REPEAT);

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
    // Add VBO to draw buffer
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, (void*)self->vbo, sizeof(vertex_rigged), 4, 0x3210);
    
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
    Mtx_Translate(&baseView, 0.0, 0.0, -self->length, true);
    Mtx_RotateZ(&baseView, M_PI * rotation, true);
    Mtx_RotateY(&baseView, M_PI, true);
    Mtx_RotateX(&baseView, M_PI * 0.5, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocModelview,  &baseView);

    // Bind textures
    C3D_TexBind(0, &self->texA);
    C3D_TexBind(1, &self->texB);

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
    tunnel[0].load(&tunnel[0]);
    
    for(int i = i; i < SEGMENT_COUNT; i++) {
        if(i % 2 == 0) {
            genSegmentDoors(&tunnel[i], "doors");
        }
        else {
            genSegmentDoors(&tunnel[i], "doorsOdd");
        }
        tunnel[i].vbo = tunnel[0].vbo;
        tunnel[i].texA = tunnel[0].texA;
        tunnel[i].texB = tunnel[0].texB;
        tunnel[i].vertCount = tunnel[0].vertCount;
    }
}

void effectTunnelDraw(C3D_RenderTarget* targetLeft, C3D_RenderTarget* targetRight, float row, float iod) {
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

    // Get Z for overall effect
    float tunnelZ = sync_get_val(syncZ, row);

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

    // Left eye
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
            tunnel[i].draw(&tunnel[i], row, modelview);
        }
        Mtx_Translate(&modelview, 0.0, 0.0, -tunnel[i].length, true);
    }

    // Right eye?
    if(iod > 0.0) {
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
    }
    //printf("Got to frameend\n");

    // Swap
    C3D_FrameEnd(0);
    //printf("swapped\n");
}
