#ifdef _WIN32
#include "libutils/IncludeWindows.h"
#else
#include <gl.h>
#endif

#include "GL/glew.h"

#include "Renderer.h"
#include "DeviceContext.h"

#include "shared/CheckedMemory.h"
#include "libutils/String.h"
#include "libutils/StandardVectors.h"
#include "libutils/BitBuffer.h"
#include "libutils/BitTwiddling.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//common colors
const ColorRGBAf White =   { 1.0f,  1.0f,  1.0f, 1.0f };
const ColorRGBAf Gray =    { 0.5f,  0.5f,  0.5f, 1.0f };
const ColorRGBAf DkGray =  { 0.25f, 0.25f, 0.25f,1.0f };
const ColorRGBAf LtGray =  { 0.75f, 0.75f, 0.75f,1.0f };
const ColorRGBAf Black =   { 0.0f,  0.0f,  0.0f, 1.0f };
const ColorRGBAf Red =     { 1.0f,  0.0f,  0.0f, 1.0f };
const ColorRGBAf DkRed =   { 0.5f,  0.0f,  0.0f, 1.0f };
const ColorRGBAf Green =   { 0.0f,  1.0f,  0.0f, 1.0f };
const ColorRGBAf DkGreen = { 0.0f,  0.5f,  0.0f, 1.0f };
const ColorRGBAf Blue =    { 0.0f,  0.0f,  1.0f, 1.0f };
const ColorRGBAf DkBlue =  { 0.0f,  0.0f,  0.5f, 1.0f };
const ColorRGBAf Cyan =    { 0.0f,  1.0f,  1.0f, 1.0f };
const ColorRGBAf Yellow =  { 1.0f,  1.0f,  0.0f, 1.0f };
const ColorRGBAf Magenta = { 1.0f,  0.0f,  1.0f, 1.0f };


typedef struct {
   StringView key;
   Uniform value;
} StringToUniform;

#define HashTableT StringToUniform
#include "libutils/HashTable_Create.h"

static int _s2uCompare(StringToUniform *e1, StringToUniform *e2) {
   return e1->key == e2->key;
}

static size_t _s2uHash(StringToUniform *p) {
   size_t out = 5031;
   out += (out << 5) + hashPtr((void*)p->key);
   return out;
}

struct Shader_t {
   String *filePath;
   ShaderParams params;
   boolean built;
   GLuint handle;
   ht(StringToUniform) *uniforms;
};

Shader *shaderCreate(const char *file, ShaderParams params) {
   Shader *out = checkedCalloc(1, sizeof(Shader));
   out->filePath = stringCreate(file);
   out->uniforms = htCreate(StringToUniform)(&_s2uCompare, &_s2uHash, NULL);
   out->params = params;
   return out;
}
void shaderDestroy(Shader *self) {
   stringDestroy(self->filePath);
   htDestroy(StringToUniform)(self->uniforms);
   checkedFree(self);
}

static unsigned int _shaderCompile(Shader *self, vec(StringPtr) *lines, int type) {
   unsigned int handle = glCreateShader(type);
   if (handle) {
      int i = 0;
      int compileStatus;
      size_t lineCount = vecSize(StringPtr)(lines);
      const GLchar **source = checkedCalloc(lineCount, sizeof(GLchar*));

      vecForEach(StringPtr, str, lines, {
         source[i++] = c_str(*str);
      });

      glShaderSource(handle, lineCount, source, NULL);
      glCompileShader(handle);

      checkedFree((void*)source);

      glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);
      if (!compileStatus) {

         //int infoLen = 0;
         //glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLen);
         //std::vector<GLchar> infoLog(infoLen);
         //glGetShaderInfoLog(handle, infoLen, &infoLen, &infoLog[0]);
         //std::string err = infoLog.data();

         return 0;
      }
   }

   return handle;
}

static unsigned int _shaderLink(unsigned int vertex, unsigned int fragment) {
   int handle = glCreateProgram();
   if (handle)
   {
      int linkStatus;
      if (vertex == -1 || fragment == -1) {
         return 0;
      }

      glBindAttribLocation(handle, (GLuint)VertexAttribute_Pos2, "aPosition");
      glBindAttribLocation(handle, (GLuint)VertexAttribute_Tex2, "aTexCoords");
      glBindAttribLocation(handle, (GLuint)VertexAttribute_Col4, "aColor");

      glAttachShader(handle, vertex);
      glAttachShader(handle, fragment);
      glLinkProgram(handle);

      glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);
      if (!linkStatus) {
         GLsizei log_length = 0;
         GLchar message[1024];
         glGetProgramInfoLog(handle, 1024, &log_length, message);

         GLsizei srclen = 0;
         GLchar vsrc[10240], fsrc[10240];
         glGetShaderSource(vertex, 10240, &srclen, vsrc);
         glGetShaderSource(fragment, 10240, &srclen, fsrc);

         return 0;
      }
   }
   return handle;
}
static void _shaderBuild(Shader *self) {
   long fSize = 0;
   byte *file = readFullFile(c_str(self->filePath), &fSize);

   if (!file) {
      return;
   }

   const char *Version = "#version 420\n";
   const char *VertexOption = "#define VERTEX\n";
   const char *FragmentOption = "#define FRAGMENT\n";
   const char *DiffuseTextureOption = "#define DIFFUSE_TEXTURE\n";
   const char *DiffuseTextureArrOption = "#define DIFFUSE_TEXTURE_ARRAY\n";
   const char *ColorAttributeOption = "#define COLOR_ATTRIBUTE\n";
   const char *RotationOption = "#define ROTATION\n";

   vec(StringPtr) *vertShader = vecCreate(StringPtr)(&stringPtrDestroy);
   vec(StringPtr) *fragShader = vecCreate(StringPtr)(&stringPtrDestroy);


   //vertex
   vecPushBack(StringPtr)(vertShader, &(String*){ stringCreate(Version) });
   vecPushBack(StringPtr)(vertShader, &(String*){ stringCreate(VertexOption) });
   if (self->params&ShaderParams_DiffuseTexture) {
      vecPushBack(StringPtr)(vertShader, &(String*){ stringCreate(DiffuseTextureOption) });
   }
   if (self->params&ShaderParams_Rotation) {
      vecPushBack(StringPtr)(vertShader, &(String*){ stringCreate(RotationOption) });
   }
   vecPushBack(StringPtr)(vertShader, &(String*){ stringCreate(file) });
   unsigned int vert = _shaderCompile(self, vertShader, GL_VERTEX_SHADER);


   //fragment
   vecPushBack(StringPtr)(fragShader, &(String*){ stringCreate(Version) });
   vecPushBack(StringPtr)(fragShader, &(String*){ stringCreate(FragmentOption) });
   if (self->params&ShaderParams_DiffuseTexture) {
      vecPushBack(StringPtr)(fragShader, &(String*){ stringCreate(DiffuseTextureOption) });
   }
   vecPushBack(StringPtr)(fragShader, &(String*){ stringCreate(file) });
   unsigned int frag = _shaderCompile(self, fragShader, GL_FRAGMENT_SHADER);

   unsigned int handle = _shaderLink(vert, frag);

   if (handle) {
      self->handle = handle;
      self->built = true;
   }

   checkedFree(file);
   vecDestroy(StringPtr)(vertShader);
   vecDestroy(StringPtr)(fragShader);
}


void shaderSetActive(Shader *self) {
   if (!self->built) {
      _shaderBuild(self);
   }
   glUseProgram(self->handle);
}

Uniform shaderGetUniform(Shader *self, StringView name) {
   if (!self->built) {
      return (Uniform)-1;
   }

   StringToUniform search = { .key = name };
   StringToUniform *found = htFind(StringToUniform)(self->uniforms, &search);
   if (!found) {
      search.value = glGetUniformLocation(self->handle, (const char*)name);
      htInsert(StringToUniform)(self->uniforms, &search);
      return search.value;
   }

   return found->value;
}
void shaderSetFloat2(Shader *self, Uniform u, const Float2 value) {
   glUniform2fv(u, 1, (float*)&value);
}
void shaderSetMatrix(Shader *self, Uniform u, const Matrix *value) {
   glUniformMatrix4fv(u, 1, false, (float*)value->data);
}
void shaderSetColor(Shader *self, Uniform u, const ColorRGBAf *value) {
   glUniform4fv(u, 1, (float*)value);
}
void shaderSetTextureSlot(Shader *self, Uniform u, const TextureSlot slot) {
   glUniform1i(u, slot);
}

struct Texture_t {   
   TextureRequest request;
   boolean isLoaded;
   GLuint glHandle;
   ColorRGBA *pixels;
   Int2 size;

   TextureManager *parent;
};

static Texture *_textureCreate(const TextureRequest request) {
   int x = 0, y = 0, comp = 0;

   if (request.path) {
      stbi_info(request.path, &x, &y, &comp);

      if (x == 0 && y == 0) {
         return NULL;
      }
   }
   

   Texture *out = checkedCalloc(1, sizeof(Texture));
   memcpy(out, &request, sizeof(TextureRequest));
   out->size.x = x;
   out->size.y = y;
   out->glHandle = -1;

   return out;
}

static void _textureRelease(Texture *self) {
   
   if (self->isLoaded) {
      glDeleteTextures(1, &self->glHandle);
   }
   
   checkedFree(self->pixels);

   self->glHandle = -1;
   self->isLoaded = false;
}

static void _textureDestroy(Texture *self) {
   if (self->pixels) {
      _textureRelease(self);
   }   
   checkedFree(self);
}

static void _textureAcquire(Texture *self) {
   if (self->request.path) {
      int comps = 0;
      byte *data = stbi_load(self->request.path, &self->size.x, &self->size.y, &comps, 4);
      if (!data) {
         return;
      }

      int pixelCount = self->size.x * self->size.y;
      self->pixels = checkedCalloc(pixelCount, sizeof(ColorRGBA));
      memcpy(self->pixels, data, pixelCount * sizeof(ColorRGBA));
      stbi_image_free(data);
   }

   glEnable(GL_TEXTURE_2D);
   glGenTextures(1, &self->glHandle);
   glBindTexture(GL_TEXTURE_2D, self->glHandle);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   switch (self->request.filterType) {
   case FilterType_Linear:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      break;
   case FilterType_Nearest:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      break;
   };

   switch (self->request.repeatType) {
   case RepeatType_Repeat:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      break;
   case RepeatType_Clamp:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      break;
   };

   //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self->size.x, self->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, self->pixels);

   glBindTexture(GL_TEXTURE_2D, 0);

   self->isLoaded = true;
}



typedef Texture *TexturePtr;

static int _texEntryCompare(TexturePtr *e1, TexturePtr *e2) {
   return (*e1)->request.filterType == (*e2)->request.filterType &&
      (*e1)->request.path == (*e2)->request.path &&
      (*e1)->request.repeatType == (*e2)->request.repeatType;
}

static size_t _texEntryHash(TexturePtr *p) {
   size_t h = 5381;
   h = (h << 5) + (h << 1) + (unsigned int)(*p)->request.repeatType;
   h = (h << 5) + (h << 1) + (unsigned int)(*p)->request.filterType;
   h = (h << 5) + (h << 1) + (unsigned int)hashPtr((void*)(*p)->request.path);
   return h;
}

static void _texEntryDestroy(TexturePtr *entry) {
   _textureDestroy(*entry); 
}

#define HashTableT TexturePtr
#include "libutils/HashTable_Create.h"

struct TextureManager_t {
   ht(TexturePtr) *textures;
};

TextureManager *textureManagerCreate(AppData *data) {
   TextureManager *out = checkedCalloc(1, sizeof(TextureManager));
   out->textures = htCreate(TexturePtr)(&_texEntryCompare, &_texEntryHash, &_texEntryDestroy);
   return out;
}
void textureManagerDestroy(TextureManager *self) {
   htDestroy(TexturePtr)(self->textures);
   checkedFree(self);
}
Texture *textureManagerGetTexture(TextureManager *self, const TextureRequest request) {
   Texture *search = (Texture*)&request;

   TexturePtr *found = htFind(TexturePtr)(self->textures, &search);
   if (!found) {
      Texture *newTex = _textureCreate(request);
      newTex->parent = self;
      if (newTex) {
         htInsert(TexturePtr)(self->textures, &newTex);
         return newTex;
      } 
      else {
         return NULL;
      }
   }

   return *found;
}

void textureBind(Texture *self, TextureSlot slot) {
   if (!self->isLoaded) {
      _textureAcquire(self);
   }
   glActiveTexture(GL_TEXTURE0 + slot);
   glBindTexture(GL_TEXTURE_2D, self->glHandle);
}
Int2 textureGetSize(Texture *self) {
   return self->size;
}
void textureDestroy(Texture *self) {
   if (self->parent) {
      htErase(TexturePtr)(self->parent->textures, &self);
   }
   else {
      _textureDestroy(self);
   }   
}

Texture *textureCreateCustom(int width, int height, RepeatType repeatType, FilterType filterType) {
   Texture *out = _textureCreate((TextureRequest){repeatType, filterType, NULL});

   out->size.x = width;
   out->size.y = height;

   out->pixels = checkedCalloc(width * height, sizeof(ColorRGBA));

   return out;
}

void textureSetPixels(Texture *self, byte *data) {

}

struct FBO_t {
   Int2 size;
   RepeatType repeat;
   FilterType filter;
   boolean loaded;
   GLuint fboHandle, texHandle;
};

static void _fboAcquire(FBO *self) {
   if (self->loaded) { 
      return; 
   }

   glGenTextures(1, &self->texHandle);
   glBindTexture(GL_TEXTURE_2D, self->texHandle);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA8,
      self->size.x, self->size.y,
      0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


   glGenFramebuffers(1, &self->fboHandle);
   glBindFramebuffer(GL_FRAMEBUFFER, self->fboHandle);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->texHandle, 0);

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);

   self->loaded = true;
}

FBO *fboCreate(Int2 size, RepeatType repeatType, FilterType filterType) {
   FBO *out = checkedCalloc(1, sizeof(FBO));

   out->size = size;
   out->repeat = repeatType;
   out->filter = filterType;

   return out;
}
void fboDestroy(FBO *self) {
   if (self->loaded) {
      glDeleteTextures(1, &self->texHandle);
      glDeleteFramebuffers(1, &self->fboHandle);
   }
   checkedFree(self);
}

void fboBindToWrite(FBO *self) {
   if (!self) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return;
   }

   if (!self->loaded) {
      _fboAcquire(self);
   }

   glBindFramebuffer(GL_FRAMEBUFFER, self->fboHandle);
}
void fboBindToRender(FBO *self, TextureSlot slot) {
   if (!self->loaded) {
      _fboAcquire(self);
   }

   glActiveTexture(GL_TEXTURE0 + slot);
   glBindTexture(GL_TEXTURE_2D, self->texHandle);
}
Int2 fboGetSize(FBO *self) {
   return self->size;
}



struct UBO_t {
   boolean built;
   size_t size;
   uintptr_t ubo;
};

static void _uboBuild(UBO *self) {
   glGenBuffers(1, &self->ubo);

   glBindBuffer(GL_UNIFORM_BUFFER, self->ubo);
   glBufferData(GL_UNIFORM_BUFFER, self->size, NULL, GL_DYNAMIC_DRAW);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);

   self->built = true;
}


UBO *uboCreate(size_t size) {
   UBO *out = checkedCalloc(1, sizeof(UBO));
   out->size = size;
   return out;
}
void uboDestroy(UBO *self) {
   if (self->built) {
      glDeleteBuffers(1, &self->ubo);
   }
   checkedFree(self);
}

void uboSetData(UBO *self, size_t offset, size_t size, void *data) {
   if (!self->built) {
      _uboBuild(self);
   }

   glBindBuffer(GL_UNIFORM_BUFFER, self->ubo);
   glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
   glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
void uboBind(UBO *self, UBOSlot slot) {
   if (!self->built) {
      _uboBuild(self);
   }

   glBindBufferBase(GL_UNIFORM_BUFFER, slot, self->ubo);
}


struct Model_t {
   byte *data;
   byte *dataNewData;
   size_t vertexSize;
   size_t vertexCount;
   ModelStreamType dataType;

   boolean built, dirtyData;

   VertexAttribute *attrs;

   GLuint vboHandle;
};

static int _vertexAttributeByteSize(VertexAttribute attr) {
   switch (attr) {
   case VertexAttribute_Tex2:
   case VertexAttribute_Pos2:
      return sizeof(Float2);
      break;
   case VertexAttribute_Col4:
      return sizeof(ColorRGBAf);
      break;
   }
   return 0;
}

static GLuint _modelGetDataType(ModelStreamType type) {
   static GLuint map[3];
   static boolean mapInit = false;
   if (!mapInit) {
      mapInit = true;
      map[ModelStreamType_Static] = GL_STATIC_DRAW;
      map[ModelStreamType_Dynamic] = GL_DYNAMIC_DRAW;
      map[ModelStreamType_Stream] = GL_STREAM_DRAW;
   }

   return map[type];
}

static void _modelBuild(Model *self) {
   glGenBuffers(1, (GLuint*)&self->vboHandle);
   glBindBuffer(GL_ARRAY_BUFFER, self->vboHandle);
   glBufferData(GL_ARRAY_BUFFER, self->vertexSize * self->vertexCount, self->data, _modelGetDataType(self->dataType));
   glBindBuffer(GL_ARRAY_BUFFER, 0);

   self->built = true;
}

Model *__modelCreate(void *data, size_t size, size_t vCount, VertexAttribute *attrs, ModelStreamType dataType) {
   Model *out = checkedCalloc(1, sizeof(Model));

   out->vertexCount = vCount;
   out->vertexSize = size;
   out->attrs = attrs;
   out->data = checkedMalloc(size * vCount);
   memcpy(out->data, data, size * vCount);
   out->dataType = dataType;

   return out;
}
void __modelUpdateData(Model *self, void *data, size_t size, size_t vCount) {
   if (size != self->vertexSize || vCount != self->vertexCount) {
      return; //picnic
   }

   if (self->dataType == ModelStreamType_Static) {
      return; //picnic
   }

   if (!self->dataNewData) {
      self->dataNewData = checkedMalloc(sizeof(size * vCount));
   }
   memcpy(self->dataNewData, data, size * vCount);
   self->dirtyData = true;
}

void modelDestroy(Model *self) {
   if (self->data) {
      checkedFree(self->data);
   }

   if (self->dataNewData) {
      checkedFree(self->dataNewData);
   }

   checkedFree(self);
}

static _addAttr(Model *self, VertexAttribute *attr, int *totalOffset) {
   
}

void modelBind(Model *self) {
   if (!self->built) {
      _modelBuild(self);
   }

   glBindBuffer(GL_ARRAY_BUFFER, self->vboHandle);

   if (self->dataNewData && self->dirtyData) {
      glBufferData(GL_ARRAY_BUFFER, self->vertexSize * self->vertexCount, self->dataNewData, _modelGetDataType(self->dataType));
      self->dirtyData = false;
   }

   //clear current attribs
   for (unsigned int i = 0; i < (unsigned int)VertexAttribute_COUNT; ++i) {
      glDisableVertexAttribArray(i);
   }

   int totalOffset = 0;
   VertexAttribute *attr = self->attrs;
   while (*attr != VertexAttribute_COUNT) {
      glEnableVertexAttribArray((unsigned int)*attr);

      int count = 0;
      int offset = totalOffset;

      totalOffset += _vertexAttributeByteSize(*attr);

      switch (*attr) {
      case VertexAttribute_Tex2:
      case VertexAttribute_Pos2:
         count = 2;
         break;
      case VertexAttribute_Col4:
         count = 4;
         break;
      }

      glVertexAttribPointer((unsigned int)*attr,
         count, GL_FLOAT, GL_FALSE, self->vertexSize, (void*)offset);

      ++attr;
   }

}
void modelDraw(Model *self, ModelRenderType renderType) {
   static GLuint map[3];
   static boolean mapInit = false;
   if (!mapInit) {
      mapInit = true;
      map[ModelRenderType_Triangles] = GL_TRIANGLES;
      map[ModelRenderType_Lines] = GL_LINES;
      map[ModelRenderType_Points] = GL_POINTS;
   }

   glDrawArrays(map[renderType], 0, self->vertexCount);
}


struct Renderer_t {
   DeviceContext *context;

   Shader *activeShader;
   Model *activeModel;
   FBO *activeFBO;
};

Renderer *rendererCreate(DeviceContext *context) {
   Renderer *out = checkedCalloc(1, sizeof(Renderer));

   out->context = context;

   return out;
}

void rendererDestroy(Renderer *self) {
   checkedFree(self);
}

//initialize the active context for rendering (things like glewinit are done here
void r_init(Renderer *self) {
   deviceContextPrepareForRendering(self->context);
   glewInit();

   glLineWidth(1.0f);
   glPointSize(1.0f);
}

//we're done pushing drawcalls, swap out the queues
void r_finish(Renderer *self) {
   //swap queues here
}

//execute the current queue and swap buffers
void r_flush(Renderer *self) {
   //execute q ueue lambdas and swap
   deviceContextCommitRender(self->context);
}

Int2 r_getSize(Renderer *self) {
   return deviceContextGetWindowSize(self->context);
}

void r_clear(Renderer *self, const ColorRGBAf *c) {
   glClearColor(c->r, c->g, c->b, c->a);
   glClear(GL_COLOR_BUFFER_BIT);
}
void r_viewport(Renderer *self, const Recti *r) {
   int winHeight = 0;
   if (self->activeFBO) {
      winHeight = fboGetSize(self->activeFBO).y;
   }
   else {
      winHeight = deviceContextGetWindowSize(self->context).y;
   }

   int width = r->right - r->left;
   int height = r->bottom - r->top;

   Recti bounds = {
      r->left,
      winHeight - r->top - height,
      width,
      height
   };

   glViewport(bounds.left, bounds.top, bounds.right, bounds.bottom);
}

void r_enableDepth(Renderer *self, boolean enabled) {
   if (enabled) {
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LEQUAL);
      glAlphaFunc(GL_GREATER, 0.5);
      glEnable(GL_ALPHA_TEST);
   }
   else {
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_ALPHA_TEST);
   }
}
void r_enableAlphaBlending(Renderer *self, boolean enabled) {
   if (enabled) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   }
   else {
      glDisable(GL_BLEND);
   }
}
void r_enableWireframe(Renderer *self, boolean enabled) {
   if (enabled) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   }
   else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }
}

void r_setShader(Renderer *self, Shader *s) {
   if (s != self->activeShader) {
      shaderSetActive(s);
      self->activeShader = s;
   }
}
void r_setFloat2(Renderer *self, StringView u, const Float2 value) {
   if (self->activeShader) {
      Uniform uni = shaderGetUniform(self->activeShader, u);
      shaderSetFloat2(self->activeShader, uni, value);
   }
}
void r_setMatrix(Renderer *self, StringView u, const Matrix *value) {
   if (self->activeShader) {
      Uniform uni = shaderGetUniform(self->activeShader, u);
      shaderSetMatrix(self->activeShader, uni, value);
   }
}
void r_setColor(Renderer *self, StringView u, const ColorRGBAf *value) {
   if (self->activeShader) {
      Uniform uni = shaderGetUniform(self->activeShader, u);
      shaderSetColor(self->activeShader, uni, value);
   }
}

void r_setTextureSlot(Renderer *self, StringView u, const TextureSlot value) {
   if (self->activeShader) {
      Uniform uni = shaderGetUniform(self->activeShader, u);
      shaderSetTextureSlot(self->activeShader, uni, value);
   }
}
void r_bindTexture(Renderer *self, Texture *t, TextureSlot slot) {
   textureBind(t, slot);
}

void r_bindFBOToWrite(Renderer *self, FBO *fbo) {
   fboBindToWrite(fbo);
   self->activeFBO = fbo;
}
void r_bindFBOToRender(Renderer *self, FBO *fbo, TextureSlot slot) {
   fboBindToRender(fbo, slot);
}

void __r_setUBOData(Renderer *self, UBO *ubo, size_t offset, size_t size, void *data) {
   uboSetData(ubo, offset, size, data);
}

void r_bindUBO(Renderer *self, UBO *ubo, UBOSlot slot) {
   uboBind(ubo, slot);
}

void r_renderModel(Renderer *self, Model *m, ModelRenderType type) {
   if (m != self->activeModel) {
      modelBind(m);
      self->activeModel = m;
   }

   modelDraw(m, type);
}