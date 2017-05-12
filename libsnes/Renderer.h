#pragma once

#include "libutils/Vector.h"
#include "libutils/Matrix.h"
#include "libutils/Defs.h"
#include "libutils/Rect.h"
#include "libutils/Strings.h"
#include "libutils/Preprocessor.h"

typedef struct AppData_t AppData;

typedef struct Shader_t Shader;
typedef uintptr_t Uniform;
typedef uintptr_t TextureSlot;

enum {
   ShaderParams_DiffuseTexture = 1 << 0,
   ShaderParams_Color = 1 << 1,
   ShaderParams_Rotation = 1 << 2,
};
typedef byte ShaderParams;

Shader *shaderCreate(const char *file, ShaderParams params);
Shader *shaderCreateFromBuffer(const char *buffer, ShaderParams params);
void shaderDestroy(Shader *self);

void shaderSetActive(Shader *self);

Uniform shaderGetUniform(Shader *self, StringView name);
void shaderSetFloat2(Shader *self, Uniform u, const Float2 value);
void shaderSetMatrix(Shader *self, Uniform u, const Matrix *value);
void shaderSetColor(Shader *self, Uniform u, const ColorRGBAf *value);
void shaderSetTextureSlot(Shader *self, Uniform u, const TextureSlot slot);

enum {
   RepeatType_Repeat,
   RepeatType_Clamp
};
typedef byte RepeatType;

enum {
   FilterType_Linear,
   FilterType_Nearest
};
typedef byte FilterType;

typedef struct {
   RepeatType repeatType;
   FilterType filterType;
   StringView path;
   char *rawBuffer;
   unsigned int rawSize;
} TextureRequest;



typedef struct Texture_t Texture;
typedef struct TextureManager_t TextureManager;


TextureManager *textureManagerCreate(AppData *data);
void textureManagerDestroy(TextureManager *self);
Texture *textureManagerGetTexture(TextureManager *self, const TextureRequest request);

Texture *textureCreateCustom(int width, int height, RepeatType repeatType, FilterType filterType);
void textureDestroy(Texture *self);

void textureSetPixels(Texture *self, byte *data);

void textureBind(Texture *self, TextureSlot slot);
Int2 textureGetSize(Texture *t);

//because why not
uint32_t textureGetGLHandle(Texture *self);

const ColorRGBA *textureGetPixels(Texture *self);



typedef struct FBO_t FBO;
FBO *fboCreate(Int2 size, RepeatType repeatType, FilterType filterType);
void fboDestroy(FBO *self);

void fboBindToWrite(FBO *self);
void fboBindToRender(FBO *self, TextureSlot slot);
Int2 fboGetSize(FBO *self);

typedef struct UBO_t UBO;
typedef uintptr_t UBOSlot;

UBO *uboCreate(size_t size);
void uboDestroy(UBO *self);

void uboSetData(UBO *self, size_t offset, size_t size, void *data);
void uboBind(UBO *self, UBOSlot slot);


typedef struct Model_t Model;
enum {
   ModelStreamType_Stream,
   ModelStreamType_Static,
   ModelStreamType_Dynamic
};
typedef byte ModelStreamType;

enum {
   ModelRenderType_Triangles, 
   ModelRenderType_Lines,
   ModelRenderType_Points
};
typedef byte ModelRenderType;

enum {
   VertexAttribute_Pos2 = 0,
   VertexAttribute_Tex2,
   VertexAttribute_Col4,
   VertexAttribute_COUNT
};
typedef byte VertexAttribute;

void glHelperBindVertexAttrributes(VertexAttribute *attrs, unsigned int vertexSize);

Model *__modelCreate(void *data, size_t size, size_t vCount, VertexAttribute *attrs, ModelStreamType dataType);
void __modelUpdateData(Model *self, void *data, size_t size, size_t vCount);

void modelDestroy(Model *self);
void modelBind(Model *self);
void modelDraw(Model *self, ModelRenderType renderType);

#define FVF_ATTRS_FUNC(NAME, ...) \
static VertexAttribute *CONCAT(NAME, _GetAttrs)() { \
   static VertexAttribute out[] = { __VA_ARGS__, VertexAttribute_COUNT }; \
   return out; \
} \
static Model *CONCAT(NAME, _CreateModel)(NAME *data, size_t vCount, ModelStreamType type) { \
   VertexAttribute *attrs = CONCAT(NAME, _GetAttrs)(); \
   return __modelCreate((void*)data, sizeof(NAME), vCount, attrs, type); \
} \
static void CONCAT(NAME, _UpdateModel)(Model *self, NAME *data, size_t vCount){ \
   __modelUpdateData(self, (void*)data, sizeof(NAME), vCount); \
}



typedef struct {
   Float2 pos2, tex2; ColorRGBAf col4;
}FVF_Pos2_Tex2_Col4;
FVF_ATTRS_FUNC(FVF_Pos2_Tex2_Col4, VertexAttribute_Pos2, VertexAttribute_Tex2, VertexAttribute_Col4)


typedef struct {
   Float2 pos2; ColorRGBAf col4;
}FVF_Pos2_Col4;
FVF_ATTRS_FUNC(FVF_Pos2_Col4, VertexAttribute_Pos2, VertexAttribute_Col4)


typedef struct Renderer_t Renderer;
typedef struct DeviceContext_t DeviceContext;

Renderer *rendererCreate(DeviceContext *context);
void rendererDestroy(Renderer *self);

//initialize the active context for rendering (things like glewinit are done here
void r_init(Renderer *self);

//we're done pushing drawcalls, swap out the queues
void r_finish(Renderer *self);

//execute the current queue and swap buffers
void r_flush(Renderer *self);

Int2 r_getSize(Renderer *self);

void r_clear(Renderer *self, const ColorRGBAf *c);
void r_viewport(Renderer *self, const Recti *r);

void r_enableDepth(Renderer *self, boolean enabled);
void r_enableAlphaBlending(Renderer *self, boolean enabled);
void r_enableWireframe(Renderer *self, boolean enabled);

void r_setShader(Renderer *self, Shader *s);
void r_setFloat2(Renderer *self, StringView u, const Float2 value);
void r_setMatrix(Renderer *self, StringView u, const Matrix *value);
void r_setColor(Renderer *self, StringView u, const ColorRGBAf *value);

void r_setTextureSlot(Renderer *self, StringView u, const TextureSlot value);
void r_bindTexture(Renderer *self, Texture *t, TextureSlot slot);

void r_bindFBOToWrite(Renderer *self, FBO *fbo);
void r_bindFBOToRender(Renderer *self, FBO *fbo, TextureSlot slot);

void __r_setUBOData(Renderer *self, UBO *ubo, size_t offset, size_t size, void *data);
#define r_setUBOData(r, ubo, data) __r_setUBOData(r, ubo, 0, sizeof(data), &data)

void r_bindUBO(Renderer *self, UBO *ubo, UBOSlot slot);

void r_renderModel(Renderer *self, Model *m, ModelRenderType type);






