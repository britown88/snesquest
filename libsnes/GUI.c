#include "GUI.h"
#include "Renderer.h"

#include "libutils/IncludeWindows.h"
#include "LogSpud.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#include "libutils/CheckedMemory.h"

#define MAX_VERTEX_MEMORY 1024 * 1024
#define MAX_ELEMENT_MEMORY 512 * 1024

#include "libsnes/snes.h"
#include "AppData.h"
#include "DeviceContext.h"
#include "FrameProfiler.h"
#include "EncodedAssets.h"

static const char *LogSpudWin = "LogSpud";
static struct nk_color _colorToNKColor(ColorRGBA in) { return nk_rgb(in.r, in.g, in.b); }

typedef struct {
   SNESColor color;
   int encodingIndex[4];  //index in the encode palette to use for the encode
}ColorMapEntry;

#define VectorT ColorMapEntry
#include "libutils/Vector_Create.h"

#define VectorT SNESColor
#include "libutils/Vector_Create.h"

//colormap is a texture-sized pre-allocated array of ints for putting the pixel indices into the output palette
static void _getUniqueColors(Texture *tex, vec(ColorMapEntry) *out, int *colorMap) {
   const ColorRGBA *pixels = textureGetPixels(tex);
   Int2 sz = textureGetSize(tex);
   int x, y;

   vecClear(ColorMapEntry)(out);
   for (y = 0; y < sz.y; ++y) {
      for (x = 0; x < sz.x; ++x) {
         int idx = y*sz.x + x;
         ColorRGBA c =  pixels[idx];
         if (c.a == 255) {
            SNESColor bit15 = snesColorConvertFrom24Bit(c);
            byte2 raw = *(byte2*)&bit15;

            boolean found = false;
            int mapColor = 0;
            vecForEach(ColorMapEntry, other, out, {
               if (raw == *(byte2*)&other->color) {
                  found = true;
                  colorMap[idx] = mapColor;
                  break;
               }
               ++mapColor;
            });

            if (!found) {
               ColorMapEntry newEntry = { 0 };
               newEntry.color = bit15;
               vecPushBack(ColorMapEntry)(out, &newEntry);
               colorMap[idx] = mapColor;
            }
         }
         else {
            colorMap[idx] = -1;
         }
      }
   }
}

typedef struct GUIWindow_t GUIWindow;
typedef struct GUIWindow_t{
   String *name;
   GUI *parent;
   void(*destroy)(GUIWindow *self); //handle any destruction of extra data
   void(*update)(GUIWindow *self, AppData *data);
} GUIWindow;


static GUIWindow *guiWindowCreate(GUI *parent, const char *name) {
   GUIWindow *out = checkedCalloc(1, sizeof(GUIWindow));
   out->parent = parent;
   out->name = stringCreate(name);
   return out;
}

static void guiWindowDestroy(GUIWindow *self) {
   if (self->destroy) {
      self->destroy(self);
   }
   stringDestroy(self->name);
   checkedFree(self);
}

typedef GUIWindow *GUIWindowPtr;
#define VectorT GUIWindowPtr
#include "libutils/Vector_Create.h"

static void _guiWindowPtrDestroy(GUIWindowPtr *self) {  
   guiWindowDestroy(*self);
}

/*
typedef struct{
   GUIWindow base;
   int someotherdata;
}MyWindow;
*/

typedef struct {
   GLuint fontTexture;
   GLuint vbo, vao, ebo;
   VertexAttribute *attrs;
} OGLData;
typedef FVF_Pos2_Tex2_Col4 GUIVertex;

struct GUI_t {
   struct nk_context ctx;
   struct nk_font_atlas atlas;
   struct nk_buffer cmds;
   struct nk_draw_null_texture null;
   OGLData ogl;

   GUIWindow *viewer, *options, *taskBar, *logSpud;
   vec(GUIWindowPtr) *dialogs;

   size_t charToolCount;
};

static void _createWindows(GUI *self);
static void _destroyWindows(GUI *self);
GUI *guiCreate() {
   GUI *out = checkedCalloc(1, sizeof(GUI));

   _createWindows(out);

   return out;
}
void guiDestroy(GUI *self) {
   _destroyWindows(self);
   nk_font_atlas_clear(&self->atlas);
   nk_free(&self->ctx);
   checkedFree(self);
}

#pragma region Init/Render

void guiBeginInput(GUI *self) {
   nk_input_begin(&self->ctx);
}
void guiEndInput(GUI *self) {
   nk_input_end(&self->ctx);
}
int guiProcessInputEvent(GUI *self, SDL_Event *evt) {
   struct nk_context *ctx = &self->ctx;
   if (evt->type == SDL_KEYUP || evt->type == SDL_KEYDOWN) {
      /* key events */
      int down = evt->type == SDL_KEYDOWN;
      const Uint8* state = SDL_GetKeyboardState(0);
      SDL_Keycode sym = evt->key.keysym.sym;

      if (sym == SDLK_RSHIFT || sym == SDLK_LSHIFT) {
         nk_input_key(ctx, NK_KEY_SHIFT, down);
      }
      else if (sym == SDLK_DELETE) {
         nk_input_key(ctx, NK_KEY_DEL, down);
      }
      else if (sym == SDLK_RETURN) {
         nk_input_key(ctx, NK_KEY_ENTER, down);
      }
      else if (sym == SDLK_TAB) {
         nk_input_key(ctx, NK_KEY_TAB, down);
      }
      else if (sym == SDLK_BACKSPACE) {
         nk_input_key(ctx, NK_KEY_BACKSPACE, down);
      }
      else if (sym == SDLK_HOME) {
         nk_input_key(ctx, NK_KEY_TEXT_START, down);
         nk_input_key(ctx, NK_KEY_SCROLL_START, down);
      }
      else if (sym == SDLK_END) {
         nk_input_key(ctx, NK_KEY_TEXT_END, down);
         nk_input_key(ctx, NK_KEY_SCROLL_END, down);
      }
      else if (sym == SDLK_PAGEDOWN) {
         nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
      }
      else if (sym == SDLK_PAGEUP) {
         nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
      }
      else if (sym == SDLK_z) {
         nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_r) {
         nk_input_key(ctx, NK_KEY_TEXT_REDO, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_c) {
         nk_input_key(ctx, NK_KEY_COPY, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_v) {
         nk_input_key(ctx, NK_KEY_PASTE, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_x) {
         nk_input_key(ctx, NK_KEY_CUT, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_b) {
         nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_e) {
         nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && state[SDL_SCANCODE_LCTRL]);
      }
      else if (sym == SDLK_UP) {
         nk_input_key(ctx, NK_KEY_UP, down);
      }
      else if (sym == SDLK_DOWN) {
         nk_input_key(ctx, NK_KEY_DOWN, down);
      }
      else if (sym == SDLK_LEFT) {
         if (state[SDL_SCANCODE_LCTRL]) {
            nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
         }
         else {
            nk_input_key(ctx, NK_KEY_LEFT, down);
         }
      }
      else if (sym == SDLK_RIGHT) {
         if (state[SDL_SCANCODE_LCTRL]) {
            nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
         }
         else {
            nk_input_key(ctx, NK_KEY_RIGHT, down);
         }
      }
      else {
         return 0;
      }

      return 1;
   }
   else if (evt->type == SDL_MOUSEBUTTONDOWN || evt->type == SDL_MOUSEBUTTONUP) {
      /* mouse button */
      int down = evt->type == SDL_MOUSEBUTTONDOWN;
      const int x = evt->button.x, y = evt->button.y;
      if (evt->button.button == SDL_BUTTON_LEFT) {
         if (evt->button.clicks > 1)
            nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
         nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
      }
      else if (evt->button.button == SDL_BUTTON_MIDDLE)
         nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down);
      else if (evt->button.button == SDL_BUTTON_RIGHT)
         nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down);
      return 1;
   }
   else if (evt->type == SDL_MOUSEMOTION) {
      /* mouse motion */
      if (ctx->input.mouse.grabbed) {
         int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y;
         nk_input_motion(ctx, x + evt->motion.xrel, y + evt->motion.yrel);
      }
      else nk_input_motion(ctx, evt->motion.x, evt->motion.y);
      return 1;
   }
   else if (evt->type == SDL_TEXTINPUT) {
      /* text input */
      nk_glyph glyph;
      memcpy(glyph, evt->text.text, NK_UTF_SIZE);
      nk_input_glyph(ctx, glyph);
      return 1;
   }
   else if (evt->type == SDL_MOUSEWHEEL) {
      /* mouse wheel */
      nk_input_scroll(ctx, nk_vec2((float)evt->wheel.x, (float)evt->wheel.y));
      return 1;
   }
   return 0;
}
static void _uploadAtlas(GUI *self, const void *image, int width, int height) {
   glGenTextures(1, &self->ogl.fontTexture);
   glBindTexture(GL_TEXTURE_2D, self->ogl.fontTexture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, image);
}
static void _fontStashBegin(GUI *self, struct nk_font_atlas **atlas){
   nk_font_atlas_init_default(&self->atlas);
   nk_font_atlas_begin(&self->atlas);
   *atlas = &self->atlas;
}
static void _fontStashEnd(GUI *self) {
   const void *image; int w, h;
   image = nk_font_atlas_bake(&self->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

   _uploadAtlas(self, image, w, h);
   nk_font_atlas_end(&self->atlas, nk_handle_id((int)self->ogl.fontTexture), &self->null);

   if (self->atlas.default_font) {
      nk_style_set_font(&self->ctx, &self->atlas.default_font->handle);
   }
}
static void _initOGLData(OGLData *self) {
   glGenBuffers(1, &self->vbo);
   glGenBuffers(1, &self->ebo);
   glGenVertexArrays(1, &self->vao);

   self->attrs = FVF_Pos2_Tex2_Col4_GetAttrs();
}
void guiInit(GUI *self) {
   nk_init_default(&self->ctx, 0);
   struct nk_font_atlas *atlas;
   _fontStashBegin(self, &atlas);
   _fontStashEnd(self);

   nk_buffer_init_default(&self->cmds);

   _initOGLData(&self->ogl);
}
void guiRender(GUI *self, Renderer *r) {
   OGLData *ogl = &self->ogl;
   void *vertices, *elements;

   glEnable(GL_BLEND);
   glBlendEquation(GL_FUNC_ADD);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);
   glEnable(GL_SCISSOR_TEST);
   glActiveTexture(GL_TEXTURE0);

   glBindVertexArray(ogl->vao);
   glBindBuffer(GL_ARRAY_BUFFER, ogl->vbo);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ogl->ebo);

   glHelperBindVertexAttrributes(ogl->attrs, sizeof(GUIVertex));

   glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_MEMORY, NULL, GL_STREAM_DRAW);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_MEMORY, NULL, GL_STREAM_DRAW);

   vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
   elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
   {
      /* fill convert configuration */
      struct nk_convert_config config;
      static const struct nk_draw_vertex_layout_element vertex_layout[] = {
         { NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(GUIVertex, pos2) },
         { NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(GUIVertex, tex2) },
         { NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(GUIVertex, col4) },
         { NK_VERTEX_LAYOUT_END }
      };
      NK_MEMSET(&config, 0, sizeof(config));
      config.vertex_layout = vertex_layout;
      config.vertex_size = sizeof(GUIVertex);
      config.vertex_alignment = NK_ALIGNOF(GUIVertex);
      config.null = self->null;
      config.circle_segment_count = 22;
      config.curve_segment_count = 22;
      config.arc_segment_count = 22;
      config.global_alpha = 1.0f;
      config.shape_AA = NK_ANTI_ALIASING_ON;
      config.line_AA = NK_ANTI_ALIASING_ON;

      /* setup buffers to load vertices and elements */
      {struct nk_buffer vbuf, ebuf;
      nk_buffer_init_fixed(&vbuf, vertices, (nk_size)MAX_VERTEX_MEMORY);
      nk_buffer_init_fixed(&ebuf, elements, (nk_size)MAX_ELEMENT_MEMORY);
      nk_convert(&self->ctx, &self->cmds, &vbuf, &ebuf, &config);}
   }

   glUnmapBuffer(GL_ARRAY_BUFFER);
   glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

   const struct nk_draw_command *cmd;
   const nk_draw_index *offset = NULL;
   Int2 winSize = r_getSize(r);

   nk_draw_foreach(cmd, &self->ctx, &self->cmds) {
      if (!cmd->elem_count) continue;
      glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);

      glScissor((GLint)(cmd->clip_rect.x),
         (GLint)((winSize.y - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h))),
         (GLint)(cmd->clip_rect.w),
         (GLint)(cmd->clip_rect.h));

      glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
      offset += cmd->elem_count;
   }
   nk_clear(&self->ctx);

   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
   glBindVertexArray(0);

   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
}

#pragma endregion

static const float OptionsWidth = 256.0f;
static const float LogSpudWidth = 300.0f;
static const float TaskBarHeight = 40.0f;

static void _viewerUpdate(GUIWindow *self, AppData *data);
static void _optionsUpdate(GUIWindow *self, AppData *data);
static void _taskBarUpdate(GUIWindow *self, AppData *data);
static void _logSpudUpdate(GUIWindow *self, AppData *data);

typedef struct FileDirectory_t FileDirectory;
#define VectorTPart FileDirectory
#include "libutils/Vector_Decl.h"

typedef struct FileDirectory_t {
   vec(StringPtr) *files;
   vec(FileDirectory) *children;
   String *path, *name;
}FileDirectory;

#define VectorTPart FileDirectory
#include "libutils/Vector_Impl.h"

static void _fileDirectoryDestroy(FileDirectory *self) {
   stringDestroy(self->path);
   stringDestroy(self->name);
   vecDestroy(StringPtr)(self->files);
   vecDestroy(FileDirectory)(self->children);
}

static void _fileDirectoryInit(FileDirectory *self, const char *path) {
   self->path = stringCreate(path);
   self->name = stringGetFilename(self->path);
   self->children = vecCreate(FileDirectory)(&_fileDirectoryDestroy);
   self->files = vecCreate(StringPtr)(&stringPtrDestroy);
}


typedef enum {
   ColorOption4,
   ColorOption16,
   ColorOption256   
}e_ColorOptions;
static const char* ColorOptions[3] = { "4-Color", "16-Color", "256-Color" };
static int _colorCountFromOption(e_ColorOptions opt) {
   switch (opt) {
   case ColorOption4: return 4;
   case ColorOption16: return 16;
   case ColorOption256: return 256;
   }
}

typedef struct {
   GUIWindow base;

   FileDirectory files;
   boolean filesLoaded;

   Texture 
      //the imported image file
      *imported, 

      //the image showing how it will be encoding
      *encodeTest; 

   // the set of unique colors in imported, contains the 
   // original color and an index into the encode palette to map to
   vec(ColorMapEntry) *importedColors; 

   // same size as imported texture, each pixel is an index into the importedColors vector
   int *importColorMap;

   //1 byte per 8x8 tile of which of the 4 palettes to use
   byte *tilePaletteMap; 

   // pixel buffer sent off to update encodeTest
   ColorRGBA *encodeTestPixels;

   // palette used to encode the final image
   vec(SNESColor) *encodePalette[4];

   // how many colors in the encode palette
   e_ColorOptions colorOption;

   //index into the unique colorEntries to use for  linking
   int selectedColorLink, selectedEncodeColor;

   //offset of impoirted image and how many 8x8 tiles to grab for the encode
   int optXOffset, optYOffset, optXTileCount, optYTileCount;
   int optShowGrid;
   int optShowColorGuide;
   int optCurrentPalette;
   int optShowTilePalettes;

   boolean paletteTileMode;
   boolean paletteTileModeClick;
   Int2 paletteTileModeClickStart;

   float grp1Resize, grp2Resize;

}CharTool;

static void _charToolUpdate(GUIWindow *self, AppData *data);
static void _charToolDestroy(GUIWindow *self);

static GUIWindow *_charToolCreate(GUI *gui) {
   CharTool *out = checkedCalloc(1, sizeof(CharTool));
   GUIWindow *outwin = (GUIWindow*)out;

   outwin->parent = gui;
   outwin->name = stringCreate("CharTool|");

   char buff[8] = { 0 };
   sprintf(buff, "%i|", gui->charToolCount++);
   stringConcat(outwin->name, buff);

   outwin->destroy = &_charToolDestroy;
   outwin->update = &_charToolUpdate;

   _fileDirectoryInit(&out->files, ".");

   out->importedColors = vecCreate(ColorMapEntry)(NULL);

   out->colorOption = ColorOption16;

   int i = 0;
   for (i = 0; i < 4; ++i) {
      out->encodePalette[i] = vecCreate(SNESColor)(NULL);
      vecResize(SNESColor)(out->encodePalette[i], _colorCountFromOption(out->colorOption), &(SNESColor){0});
   }
   

   out->selectedColorLink = -1;
   out->optShowColorGuide = true;

   return outwin;
}
void _charToolDestroy(GUIWindow *_self) {
   CharTool *self = (CharTool*)_self;
   _fileDirectoryDestroy(&self->files);
   if (self->imported) {
      textureDestroy(self->imported);
      textureDestroy(self->encodeTest);
      checkedFree(self->importColorMap);
      checkedFree(self->encodeTestPixels);
      checkedFree(self->tilePaletteMap);
   }

   vecDestroy(ColorMapEntry)(self->importedColors);

   int i = 0;
   for (i = 0; i < 4; ++i) {
      vecDestroy(SNESColor)(self->encodePalette[i]);
   }
}

static int encTestLineCounter = 0, encTestLineTimer = 0;
static void _updateEncodeTest(CharTool *self) {
   ColorRGBA *pixels = self->encodeTestPixels;
   int x = 0, y = 0;
   Int2 encSize = textureGetSize(self->encodeTest);
   Int2 impSize = textureGetSize(self->imported);

   int pixelIdx = 0;//track the pixel in the target texture
   for (y = self->optYOffset; y < self->optYOffset + self->optYTileCount * 8; ++y) {
      for (x = self->optXOffset; x < self->optXOffset + self->optXTileCount * 8; ++x) {
         ColorRGBA c = { 0 };

         //skip poitential pixels outside the imported image
         if ( y >= 0 && x >= 0 && y < impSize.y && x < impSize.x) {
            int mapIdx = y * impSize.x + x;
            int colorIndex = self->importColorMap[mapIdx];

            int tileX = (x - self->optXOffset) / 8;
            int tileY = (y - self->optYOffset) / 8;
            int tileIdx = tileY * self->optXTileCount + tileX;
            int tilePalette = self->tilePaletteMap[tileIdx];

            //skip transparent
            if (colorIndex >= 0) {
               ColorMapEntry *entry = vecAt(ColorMapEntry)(self->importedColors, colorIndex);
               boolean selected = false;
               if (colorIndex == self->selectedColorLink) {
                  if (tilePalette == self->optCurrentPalette && encTestLineCounter % 2 && self->optShowColorGuide) {
                     c = (ColorRGBA) { 0, 255, 0, 255 };
                     selected = true;
                  }
               }

               if (!selected) {
                  if (entry->encodingIndex[tilePalette] > 0) {
                     SNESColor *ec = vecAt(SNESColor)(self->encodePalette[tilePalette], entry->encodingIndex[tilePalette]);
                     c = snesColorConverTo24Bit(*ec);
                  }
                  else if (entry->encodingIndex[tilePalette] == -1 && self->optShowColorGuide) {
                     /*if (((i/encSize.x) % 3) == (encTestLineCounter % 3)) {*/
                     if (encTestLineCounter % 2) {
                        c = (ColorRGBA) { 255, 0, 0, 255 };
                     }
                     else {
                        c = snesColorConverTo24Bit(entry->color);
                     }
                  }
               }
            }
         }

         pixels[pixelIdx++] = c;
      }
   }

   textureSetPixels(self->encodeTest, (byte*)self->encodeTestPixels);

}

// fills encodePalette withe first colors it finds in the encodetest subimage
// and links these to the imported color set
static void _smartFillEncodedPalette(CharTool *self) {
   
   int x = 0, y = 0;
   Int2 encSize = textureGetSize(self->encodeTest);
   Int2 impSize = textureGetSize(self->imported);

   int pIdx = 0;//self->optCurrentPalette;
   for (pIdx = 0; pIdx < 4; ++pIdx) {
      int current = 1;

      //clear existing
      size_t paletteSize = vecSize(SNESColor)(self->encodePalette[pIdx]);
      vecClear(SNESColor)(self->encodePalette[pIdx]);
      vecResize(SNESColor)(self->encodePalette[pIdx], paletteSize, &(SNESColor){0});

      //reset links
      vecForEach(ColorMapEntry, entry, self->importedColors, {
         entry->encodingIndex[pIdx] = -1;
      });

      for (y = self->optYOffset; y < self->optYOffset + self->optYTileCount * 8 && y < impSize.y; ++y) {
         for (x = self->optXOffset; x < self->optXOffset + self->optXTileCount * 8 && x < impSize.x; ++x) {
            int mapIdx = y * impSize.x + x;
            int entryIndex = self->importColorMap[mapIdx];

            if (y < 0 || x < 0) {
               continue;
            }

            int tileX = (x - self->optXOffset) / 8;
            int tileY = (y - self->optYOffset) / 8;
            int tileIdx = tileY * self->optXTileCount + tileX;
            int tilePalette = self->tilePaletteMap[tileIdx];

            if (tilePalette != pIdx) {
               continue;
            }

            if (entryIndex >= 0) {
               ColorMapEntry *entry = vecAt(ColorMapEntry)(self->importedColors, entryIndex);
               //now fit it onto the palette and link it up

               int p = 1;
               for (p = 1; p < current; ++p) {
                  SNESColor colorAt = *vecAt(SNESColor)(self->encodePalette[pIdx], p);
                  if (*(byte2*)&entry->color == *(byte2*)&colorAt) {
                     //already found
                     break;
                  }
               }

               if (p == current) {
                  SNESColor *currentColor = vecAt(SNESColor)(self->encodePalette[pIdx], current);
                  *currentColor = entry->color;
                  entry->encodingIndex[pIdx] = current;
                  ++current;
               }

               if (current >= paletteSize) {
                  break;
               }
            }
         }

         if (current >= paletteSize) {
            break;
         }
      }

   }


   
}

static void _updateEncodeTestSize(CharTool *self) {
   if (!self->imported) {
      return;
   }


   if (self->encodeTest) {
      textureDestroy(self->encodeTest);
      checkedFree(self->encodeTestPixels);
      checkedFree(self->tilePaletteMap);
   }

   Int2 encTestSize = { self->optXTileCount * 8, self->optYTileCount * 8 };
   self->encodeTestPixels = checkedCalloc(1, encTestSize.x * encTestSize.y * sizeof(ColorRGBA));
   self->encodeTest = textureCreateCustom(encTestSize.x, encTestSize.y, RepeatType_Clamp, FilterType_Nearest);
   self->tilePaletteMap = checkedCalloc(self->optXTileCount * self->optYTileCount, sizeof(byte));
}

//Import a texture from a file update all options and such back to normal
static void _importTextureFromFile(CharTool *self, String *file) {
   TextureRequest request = {
      .repeatType = RepeatType_Clamp,
      .filterType = FilterType_Nearest,
      .path = stringIntern(c_str(file))
   };

   Texture *imported = textureCreate(request);
   if (!imported) {
      //TODO: Show failure message
      return;
   }

   //free existing buffers
   if (self->imported) {
      textureDestroy(self->imported);
      checkedFree(self->importColorMap);
   }

   self->imported = imported;
   Int2 texSize = textureGetSize(self->imported);

   self->importColorMap = checkedCalloc(1, texSize.x * texSize.y * sizeof(int));
   self->optXOffset = 0;
   self->optYOffset = 0;
   self->optXTileCount = texSize.x / 8 + (texSize.x % 8 ? 1 : 0);
   self->optYTileCount = texSize.y / 8 + (texSize.y % 8 ? 1 : 0);
   self->selectedColorLink = -1;

   _updateEncodeTestSize(self);
   _getUniqueColors(self->imported, self->importedColors, self->importColorMap);

   vecForEach(ColorMapEntry, entry, self->importedColors, {
      int i = 0;
      for (i = 0; i < 4; ++i) {
         entry->encodingIndex[i] = -1;
      }
      
   });

   _smartFillEncodedPalette(self);
}



void _createWindows(GUI *self) {
   self->viewer = guiWindowCreate(self, "Viewer");
   self->viewer->update = &_viewerUpdate;

   self->options = guiWindowCreate(self, "Options");
   self->options->update = &_optionsUpdate;

   self->taskBar = guiWindowCreate(self, "Taskbar");
   self->taskBar->update = &_taskBarUpdate;

   self->logSpud = guiWindowCreate(self, LogSpudWin);
   self->logSpud->update = &_logSpudUpdate;

   self->dialogs = vecCreate(GUIWindowPtr)(&_guiWindowPtrDestroy);
}

void _destroyWindows(GUI *self){
   guiWindowDestroy(self->viewer);
   guiWindowDestroy(self->options);
   guiWindowDestroy(self->taskBar);
   guiWindowDestroy(self->logSpud);
   vecDestroy(GUIWindowPtr)(self->dialogs);
}


void _viewerUpdate(GUIWindow *self, AppData *data) {
   struct nk_context *ctx = &self->parent->ctx;
   Int2 windowSize = data->window->windowResolution;
   Float2 optionsSize = { OptionsWidth, (float)windowSize.y };

   struct nk_rect viewerRect = nk_rect(0, 0, windowSize.x - optionsSize.x, (float)windowSize.y);
   if (nk_begin(ctx, c_str(self->name), viewerRect,
      NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE  ))
   {
      viewerRect = nk_window_get_bounds(ctx);

      if (!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
         //if (viewerRect.x < 0 || viewerRect.y < 0) { 
         //   viewerRect.w = windowSize.x - optionsSize.x; 
         //}
         if (viewerRect.x < 0) { viewerRect.x = 0; }
         if (viewerRect.y < 0) { viewerRect.y = 0; }

         viewerRect.h = (viewerRect.w * 9) / 16.0f + 50;
         nk_window_set_bounds(ctx, viewerRect);
      }

      if (nk_window_has_focus(ctx) && nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, viewerRect)) {


         viewerRect.w = windowSize.x - optionsSize.x; 

         if (!nk_window_is_hidden(ctx, LogSpudWin)) {
            viewerRect.w -= LogSpudWidth;
         }


         viewerRect.x = 0;
         viewerRect.y = 0;
         viewerRect.h = (viewerRect.w * 9) / 16.0f + 50;
         nk_window_set_bounds(ctx, viewerRect);
      }

      struct nk_rect winBounds = nk_window_get_content_region(ctx);
      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));

      float h = (winBounds.w * 9) / 16.0f;
      nk_layout_row_begin(ctx, NK_DYNAMIC, h, 1);
      nk_layout_row_push(ctx, 1.0f);

      enum nk_widget_layout_states state;
      struct nk_rect bounds;
      state = nk_widget(&bounds, ctx);
      if (state) {
         uint32_t handle = textureGetGLHandle(data->snesTex);
         struct nk_image img = nk_image_id(handle);
         nk_draw_image(nk_window_get_canvas(ctx), bounds, &img, nk_rgb(255, 255, 255));

      }

      nk_layout_row_end(ctx);
      nk_style_pop_vec2(ctx);
   }
   nk_end(ctx);

}


static void _buildOptionsPalette(GUIWindow *self, AppData *data) {
   struct nk_context *ctx = &self->parent->ctx;

   static int selectedPalette = 0;
   static SNESColor palette[256] = { 0 };

   if (nk_tree_push(ctx, NK_TREE_TAB, "Palette", NK_MINIMIZED)) {

      static boolean firstLoad = true;
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

      //if (firstLoad) {
      memcpy(palette, data->snes->cgram.bgPalette256.colors, sizeof(SNESColor) * 256);
      firstLoad = false;
      //}

      //palette table
      const float palRowHeight = 12, palRectWidth = 12;
      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 2));
      nk_layout_row_begin(ctx, NK_STATIC, palRowHeight, 2);
      int y = 0;
      for (y = 0; y < 16; ++y) {
         nk_layout_row_push(ctx, 20);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "%i: ", y);

         nk_layout_row_push(ctx, palRectWidth * 16);

         enum nk_widget_layout_states state;
         struct nk_rect bounds;

         state = nk_widget(&bounds, ctx);
         if (state) {
            int x = 0;
            for (x = 0; x < 16; ++x) {
               boolean selected = selectedPalette == y * 16 + x;

               struct nk_rect pBounds = nk_rect(bounds.x, bounds.y, palRectWidth, bounds.h);
               pBounds.x += x * palRectWidth;

               ColorRGBA c = snesColorConverTo24Bit(palette[y * 16 + x]);
               nk_fill_rect(canvas, pBounds, 0, _colorToNKColor(c));

               if (nk_input_mouse_clicked(&ctx->input, NK_BUTTON_LEFT, pBounds)) {
                  selectedPalette = y * 16 + x;
               }

               ++pBounds.x; ++pBounds.y;
               pBounds.w -= 2;
               pBounds.h -= 2;

               if (selected) {
                  nk_stroke_rect(canvas, pBounds, 0, 2, nk_rgb(255, 255, 255));
               }
            }
         }
      }

      nk_layout_row_end(ctx);
      nk_style_pop_vec2(ctx);


      if (nk_tree_push(ctx, NK_TREE_NODE, "Edit Color", NK_MAXIMIZED)) {
         //nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
         nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
         nk_layout_row_push(ctx, 100);
         nk_labelf(ctx, NK_TEXT_RIGHT, "Selected: %i", selectedPalette);
         nk_layout_row_push(ctx, 50);

         ColorRGBA c = snesColorConverTo24Bit(palette[selectedPalette]);
         nk_button_color(ctx, _colorToNKColor(c));
         nk_layout_row_end(ctx);

         nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
         nk_layout_row_push(ctx, 15);
         nk_label(ctx, "R", NK_TEXT_LEFT);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].r = nk_slide_int(ctx, 0, palette[selectedPalette].r, 31, 1);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].r = (nk_byte)nk_propertyi(ctx, "", 0, palette[selectedPalette].r, 31, 1, 1);
         nk_layout_row_end(ctx);

         nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
         nk_layout_row_push(ctx, 15);
         nk_label(ctx, "G", NK_TEXT_LEFT);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].g = nk_slide_int(ctx, 0, palette[selectedPalette].g, 31, 1);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].g = (nk_byte)nk_propertyi(ctx, "", 0, palette[selectedPalette].g, 31, 1, 1);
         nk_layout_row_end(ctx);

         nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
         nk_layout_row_push(ctx, 15);
         nk_label(ctx, "B", NK_TEXT_LEFT);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].b = nk_slide_int(ctx, 0, palette[selectedPalette].b, 31, 1);
         nk_layout_row_push(ctx, 70);
         palette[selectedPalette].b = (nk_byte)nk_propertyi(ctx, "", 0, palette[selectedPalette].b, 31, 1, 1);
         nk_layout_row_end(ctx);

         nk_tree_pop(ctx);
      }

      nk_tree_pop(ctx);
      memcpy(data->snes->cgram.bgPalette256.colors, palette, sizeof(SNESColor) * 256);
   }
}

void _optionsUpdate(GUIWindow *self, AppData *data) {
   struct nk_context *ctx = &self->parent->ctx;

   Int2 windowSize = data->window->windowResolution;
   Float2 optionsSize = { 256.0f, (float)windowSize.y };

   struct nk_rect optRect = nk_rect(windowSize.x - optionsSize.x, 0, optionsSize.x, optionsSize.y);
   static boolean openDemo = false;
   
   if (nk_begin(ctx, c_str(self->name), optRect,
     NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE))
   {
      _buildOptionsPalette(self, data);

      if (nk_tree_push(ctx, NK_TREE_TAB, "Tools", NK_MINIMIZED)) {
         nk_layout_row_dynamic(ctx, 20, 1);

         if (nk_button_label(ctx, "CharTool")) {
            GUIWindow *ctool = _charToolCreate(self->parent);
            vecPushBack(GUIWindowPtr)(self->parent->dialogs, &ctool);
         }
         if (nk_button_label(ctx, "Nuklear Demo")) {
            openDemo = true;
            nk_window_show(ctx, "Overview", NK_SHOWN);
            nk_window_set_focus(ctx, "Overview");
         }
         if (nk_button_label(ctx, "Test Logging")) {
            LOG("GUI", LOG_INFO, "Here's some info");
            LOG("GUI", LOG_INFOBLUE, "Here's some info that is blue");
            LOG("GUI", LOG_WARN, "Hey you shhould take a look at this");
            LOG("GUI", LOG_SUCCESS, "Hey it worked");
            LOG("GUI", LOG_ERR, "Oh fuck Oh fuck Oh fuck Oh fuck Oh fuck Oh fuck Oh fuck ");
         }


         nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Testing", NK_MINIMIZED)) {

         nk_layout_row_dynamic(ctx, 20, 1);
         nk_checkbox_label(ctx, "Debug Render", (int*)&data->snesRenderWhite);

         nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
         nk_layout_row_push(ctx, 0.35f);
         nk_labelf(ctx, NK_TEXT_RIGHT, "TestX: %i", data->testX);
         nk_layout_row_push(ctx, 0.65f);
         data->testX = nk_slide_int(ctx, -256, data->testX, 256, 1);
         nk_layout_row_end(ctx);

         nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
         nk_layout_row_push(ctx, 0.35f);
         nk_labelf(ctx, NK_TEXT_RIGHT, "TestY: %i", data->testY);
         nk_layout_row_push(ctx, 0.65f);
         data->testY = nk_slide_int(ctx, 0, data->testY, 255, 1);
         nk_layout_row_end(ctx);

         nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Profiling", NK_MINIMIZED)) {
         Microseconds full = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_FULL_FRAME);
         Microseconds update = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_UPDATE);
         Microseconds render = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_RENDER);
         Microseconds gameUpdate = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_GAME_UPDATE);
         Microseconds gui = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_GUI_UPDATE);
         Microseconds snes = frameProfilerGetProfileAverage(data->frameProfiler, PROFILE_SNES_RENDER);
         struct nk_rect wBounds = { 0 };
         static nk_size usCap = 33333;

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);       
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Frame: %05.2f", full/1000.0f);
         nk_progress(ctx, (nk_size*)&full, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Total frame time (ms)");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Step: %05.2f", update / 1000.0f);
         nk_progress(ctx, (nk_size*)&update, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Frame minus fps-waits");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Game: %05.2f", gameUpdate / 1000.0f);
         nk_progress(ctx, (nk_size*)&gameUpdate, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Game Step");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "SNES: %05.2f", snes / 1000.0f);
         nk_progress(ctx, (nk_size*)&snes, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "SNES Software Render");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Rend: %05.2f", render / 1000.0f);
         nk_progress(ctx, (nk_size*)&render, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Full Render (incl GUI)");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "GUI: %05.2f", gui / 1000.0f);
         nk_progress(ctx, (nk_size*)&gui, usCap, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Time spent in Nuklear");
         }

         nk_tree_pop(ctx);
      }

      nk_layout_row_begin(ctx, NK_DYNAMIC, 55, 2);

      nk_layout_row_push(ctx, 0.25f);
      static Texture *spud = NULL;
      if (!spud) {
         TextureRequest request = {
            .repeatType = RepeatType_Clamp,
            .filterType = FilterType_Nearest,
            .rawBuffer = enc_SpudTexture,
            .rawSize = sizeof(enc_SpudTexture)
         };         spud = textureManagerGetTexture(data->textureManager, request);

      }
      if (nk_button_image(ctx, nk_image_id((int)textureGetGLHandle(spud)))) {

         
         if (nk_window_is_hidden(ctx, LogSpudWin)) {
            nk_window_show(ctx, LogSpudWin, NK_SHOWN);
            nk_window_set_focus(ctx, LogSpudWin);
         } 
         else{
            

            nk_window_show(ctx, LogSpudWin, NK_HIDDEN);
            
         }
      }

      nk_layout_row_push(ctx, 0.75f);
      enum nk_widget_layout_states state;
      struct nk_rect bounds;
      state = nk_widget(&bounds, ctx);
      if (state) {
         static Texture *logo = NULL;
         if (!logo) {
            TextureRequest request = {
               .repeatType = RepeatType_Clamp,
               .filterType = FilterType_Nearest,
               .rawBuffer = enc_LogoTexture,
               .rawSize = sizeof(enc_LogoTexture)
            };
            logo = textureManagerGetTexture(data->textureManager, request);            
         }

         struct nk_image img = nk_image_id((int)textureGetGLHandle(logo));
         Int2 sz = textureGetSize(logo);

         bounds.h = bounds.w * (sz.y / (float)sz.x);
         nk_draw_image(nk_window_get_canvas(ctx), bounds, &img, nk_rgba(255, 255, 255, 128));
      }

      nk_layout_row_end(ctx);

   }
   nk_end(ctx);

   if (openDemo) {
      nuklear_overview(ctx);
   }
}
void _taskBarUpdate(GUIWindow *self, AppData *data) {
   struct nk_context *ctx = &self->parent->ctx;
   Int2 windowSize = data->window->windowResolution;

   struct nk_rect taskBarRect = nk_rect(0, windowSize.y - TaskBarHeight, windowSize.x - OptionsWidth, TaskBarHeight);
   if (nk_begin(ctx, c_str(self->name), taskBarRect, NK_WINDOW_BORDER)) {
      vec(GUIWindowPtr) *dlgs = self->parent->dialogs;
      size_t dlgCount = vecSize(GUIWindowPtr)(dlgs);

      nk_layout_row_static(ctx, 20.0f, 100, dlgCount);

      vecForEach(GUIWindowPtr, dlgPtr, dlgs, {
         GUIWindow *dlg = *dlgPtr;
         if (nk_button_label(ctx, c_str(dlg->name))) {
            nk_window_set_focus(ctx, c_str(dlg->name));
         }
      });
   }
   nk_end(ctx);
}
void _logSpudUpdate(GUIWindow *self, AppData *data) {
   struct nk_context *ctx = &self->parent->ctx;
   Int2 winSize = data->window->windowResolution;
   static const Int2 dlgSize = { 400, 600 };

   struct nk_rect winRect = nk_rect(
      winSize.x - OptionsWidth - LogSpudWidth,
      0, LogSpudWidth, winSize.y - TaskBarHeight
   );

   nk_flags winFlags = 
      NK_WINDOW_CLOSABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE |
      NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

   static boolean firstLoad = true;
   if (firstLoad) {
      winFlags |= NK_WINDOW_HIDDEN;
      firstLoad = false;
   }


   if (nk_begin(ctx, c_str(self->name), winRect, winFlags))
   {
      struct nk_panel *pnl = nk_window_get_panel(ctx);

      if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, pnl->bounds)) {
         struct nk_rect r = nk_rect(
            winSize.x - OptionsWidth - LogSpudWidth,
            0, LogSpudWidth, winSize.y - TaskBarHeight
         );
         nk_window_set_size(ctx, nk_vec2(r.w, r.h));
         nk_window_set_position(ctx, nk_vec2(r.x, r.y));
      }



      static int showInfo = 1;
      static int showWarn = 1;
      static int showSuccess = 1;
      static int showError = 1;

      nk_layout_row_dynamic(ctx, 20.0f, 4);
      nk_selectable_label(ctx, "Info", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_CENTERED, &showInfo);
      nk_selectable_label(ctx, "Warn", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_CENTERED, &showWarn);
      nk_selectable_label(ctx, "Success", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_CENTERED, &showSuccess);
      nk_selectable_label(ctx, "Error", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_CENTERED, &showError);

      nk_layout_row_dynamic(ctx, pnl->bounds.h - 40, 1);
      if (nk_group_begin(ctx, "loggrp", NK_WINDOW_BORDER)) {
         static size_t logCount = 0;
         

         nk_layout_row_dynamic(ctx, 15, 1);
         vec(LogSpudEntry) *log = logSpudGet(data->log);
         

         vecForEach(LogSpudEntry, e, log, {
            struct nk_color c = { 0 };
            switch (e->level) {
            case LOG_INFO: c = nk_rgb(255, 255, 255); if (!showInfo) { continue; } break;
            case LOG_INFOBLUE: c = nk_rgb(75, 88, 255); if (!showInfo) { continue; } break;
            case LOG_WARN: c = nk_rgb(255, 242, 135); if (!showWarn) { continue; } break;
            case LOG_SUCCESS: c = nk_rgb(70, 196, 70); if (!showSuccess) { continue; } break;
            case LOG_ERR: c = nk_rgb(219, 43, 60); if (!showError) { continue; } break;
            }

            nk_labelf_colored(ctx, NK_TEXT_ALIGN_LEFT, c, "|%s| %s", e->tag, c_str(e->msg));
         });

         size_t eCount = vecSize(LogSpudEntry)(log);
         if (eCount != logCount) {
            *ctx->current->layout->offset_y = (int)INT_MAX;
            logCount = eCount;
         }

         nk_group_end(ctx);
      }
   }
   nk_end(ctx);
}


static void _loadFiles(FileDirectory *root) {
   vec(StringPtr) *dirs = 0, *files = 0;
   const char *path = c_str(root->path);
   deviceContextListFiles(path, DC_FILE_DIR_ONLY, &dirs, NULL);
   deviceContextListFiles(path, DC_FILE_FILE_ONLY, &files, "png");

   if (dirs) { 
      vecForEach(StringPtr, str, dirs, { 
         FileDirectory newDir = {0};
         _fileDirectoryInit(&newDir, c_str(*str));
         _loadFiles(&newDir);
         vecPushBack(FileDirectory)(root->children, &newDir);
      });
      vecDestroy(StringPtr)(dirs); 
   }

   if (files) { 
      vecForEach(StringPtr, str, files, { 
         String *strcpy = stringCopy(*str);
         vecPushBack(StringPtr)(root->files, &strcpy);
      });
      vecDestroy(StringPtr)(files);
   }
}
static String *_buildFileTree(struct nk_context *ctx, FileDirectory *root) {
   String *out = NULL;
   vecForEach(FileDirectory, dir, root->children, {

      if (nk_tree_push_id(ctx, NK_TREE_TAB, c_str(dir->name), NK_MINIMIZED, (int)dir)) {         
         out = _buildFileTree(ctx, dir);

         vecForEach(StringPtr, f, dir->files, {
            String *justfile = stringGetFilename(*f);
            nk_layout_row_dynamic(ctx, 20, 1);
            if (nk_button_label(ctx, c_str(justfile))) {
               out = *f;
            }
            stringDestroy(justfile);
         });         

         nk_tree_pop(ctx);
      }
   });

   return out;
}

void _charToolUpdate(GUIWindow *selfwin, AppData *data) {
   struct nk_context *ctx = &selfwin->parent->ctx;
   CharTool *self = (CharTool*)selfwin;

   Int2 winSize = data->window->nativeResolution;
   static const Int2 dlgSize = { 800, 600 };
   boolean allowRightClickCancel = true;

   struct nk_rect winRect = nk_rect(
      (winSize.x - dlgSize.x) / 2.0f,
      (winSize.y - dlgSize.y) / 2.0f,
      (float)dlgSize.x, (float)dlgSize.y);

   static nk_flags winFlags = 
      NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER |
      NK_WINDOW_TITLE | NK_WINDOW_MOVABLE |
      NK_WINDOW_CLOSABLE | NK_WINDOW_SCALABLE;

   const float palHeight = 75.0f;
   const float optGroupWidth = 200.0f;
   const float spacer = 10.0f;

   if (nk_begin(ctx, c_str(selfwin->name), winRect, winFlags)){

      //menu
      nk_menubar_begin(ctx);
      nk_layout_row_begin(ctx, NK_STATIC, 25, 1);
      nk_layout_row_push(ctx, 45);
      if (nk_menu_begin_label(ctx, "Import", NK_TEXT_LEFT, nk_vec2(300.0f, 5000.0f))) {
         if (!self->filesLoaded) {
            _loadFiles(&self->files);
            self->filesLoaded = true;
         }
         nk_style_push_flags(ctx, &ctx->style.button.text_alignment, NK_TEXT_ALIGN_LEFT);

         String *found = NULL;
         if (found = _buildFileTree(ctx, &self->files)) {
            _importTextureFromFile(self, found);
            nk_menu_close(ctx);
         }

         nk_style_pop_flags(ctx);

         nk_menu_end(ctx);
      }
      else if (self->filesLoaded) {         
         _fileDirectoryDestroy(&self->files);
         _fileDirectoryInit(&self->files, ".");
         self->filesLoaded = false;
      }

      nk_menubar_end(ctx);

      //split panel
      struct nk_rect area = nk_window_get_content_region(ctx);
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
      float imgGroupWidth = ( area.w - optGroupWidth - (spacer * 2) - 20) / 2.0f;

      nk_layout_row_begin(ctx, NK_STATIC, area.h - palHeight - 8, 5);
      nk_layout_row_push(ctx, imgGroupWidth + self->grp1Resize);
      if (nk_group_begin(ctx, "Original", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

         if (self->imported) {
            struct nk_panel *pnl = nk_window_get_panel(ctx);

            //nk_layout_row_dynamic(ctx, 0, 1);
            struct nk_rect rbounds = pnl->bounds;
            //nk_layout_peek(&rbounds, ctx);            

            struct nk_image img = nk_image_id((int)textureGetGLHandle(self->imported));
            Int2 impSize = textureGetSize(self->imported);
            float ratio = impSize.x / (float)impSize.y;

            nk_layout_row_dynamic(ctx, pnl->bounds.h - 15, 1);

            struct nk_rect bounds;
            if (nk_widget(&bounds, ctx)) {
               bounds.w = bounds.h * ratio;
               nk_draw_image(canvas, bounds, &img, nk_rgb(255, 255, 255));
            }
         }

         nk_group_end(ctx);
      }

      //spacing
      nk_layout_row_push(ctx, spacer);
      struct nk_rect wbounds = nk_widget_bounds(ctx);
      const struct nk_input *in = &ctx->input;
      if ((nk_input_is_mouse_hovering_rect(in, wbounds) ||
         nk_input_is_mouse_prev_hovering_rect(in, wbounds)) &&
         nk_input_is_mouse_down(in, NK_BUTTON_LEFT)) {
         self->grp1Resize = NK_MAX(NK_MIN(self->grp1Resize + in->mouse.delta.x, imgGroupWidth - 10.0f), -imgGroupWidth + 10.0f);
         self->grp2Resize = NK_MAX(NK_MIN(self->grp2Resize - in->mouse.delta.x, imgGroupWidth - 10.0f), -imgGroupWidth + 10.0f);
      }
      nk_spacing(ctx, 1);

      nk_layout_row_push(ctx, imgGroupWidth + self->grp2Resize);
      if (nk_group_begin(ctx, "Encoding", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

         if (self->imported) {
            struct nk_panel *pnl = nk_window_get_panel(ctx);
            struct nk_rect rbounds = pnl->bounds;       

            struct nk_image img = nk_image_id((int)textureGetGLHandle(self->encodeTest));
            Int2 imgSize = textureGetSize(self->encodeTest);
            float ratio = imgSize.x / (float)imgSize.y;

            _updateEncodeTest(self);
            nk_layout_row_dynamic(ctx, pnl->bounds.h - 15, 1);

            struct nk_rect bounds;
            if (nk_widget(&bounds, ctx)) {
               bounds.w = bounds.h * ratio;
               bounds.w *= 1.16666f;
               nk_draw_image(canvas, bounds, &img, nk_rgb(255, 255, 255));
               
               float tWidth = bounds.w / self->optXTileCount;
               float tHeight = bounds.h / self->optYTileCount;
               struct nk_color lineColor = nk_rgb(0, 0, 255);
               int x, y;

               if (self->paletteTileMode) {
                  if (nk_input_is_mouse_hovering_rect(in, bounds)) {
                     if (nk_input_is_mouse_down(in, NK_BUTTON_LEFT)) {
                        if (!self->paletteTileModeClick) {
                           self->paletteTileModeClick = true;
                           self->paletteTileModeClickStart = (Int2) { in->mouse.pos.x, in->mouse.pos.y };
                        }

                        nk_stroke_rect(canvas, nk_rect(
                           self->paletteTileModeClickStart.x, 
                           self->paletteTileModeClickStart.y,
                           in->mouse.pos.x - self->paletteTileModeClickStart.x,
                           in->mouse.pos.y - self->paletteTileModeClickStart.y
                           ), 0, 3, lineColor);
                     }
                     else if(self->paletteTileModeClick) {
                        self->paletteTileModeClick = false;

                        int xTile = (self->paletteTileModeClickStart.x - bounds.x) / tWidth;
                        int yTile = (self->paletteTileModeClickStart.y - bounds.y) / tHeight;
                        int xTileCount = (in->mouse.pos.x - self->paletteTileModeClickStart.x) / tWidth + 1;
                        int yTileCount = (in->mouse.pos.y - self->paletteTileModeClickStart.y) / tHeight + 1;

                        for (x = 0; x < xTileCount; ++x) {
                           for (y = 0; y < yTileCount; ++y) {
                              Int2 tile = { x + xTile, y + yTile };
                              if (tile.x >= 0 && tile.x < self->optXTileCount && 
                                 tile.y >= 0 && tile.y < self->optYTileCount) {
                                 byte *palIndex = self->tilePaletteMap + (tile.y * self->optXTileCount + tile.x);
                                 *palIndex = self->optCurrentPalette;
                              }
                           }
                        }
                     }
                  }
               }

               for (y = 0; y < self->optYTileCount; ++y) {
                  for (x = 0; x < self->optXTileCount; ++x) {
                     byte *palIndex = self->tilePaletteMap + (y * self->optXTileCount + x);
                     struct nk_rect tileBounds = nk_rect(bounds.x + tWidth*x, bounds.y + tHeight*y, tWidth, tHeight);


                     if (self->optShowTilePalettes && *palIndex == self->optCurrentPalette) {
                        nk_stroke_rect(canvas, tileBounds, 0, 3, lineColor);
                     }
                  }
               }

               nk_stroke_rect(canvas, bounds, 0, 1, nk_rgb(255, 255, 255));

               if (self->optShowGrid) {
                  int i = 1;
                  struct nk_color lineColor = nk_rgb(255, 255, 255);

                  for (i = 1; i < self->optXTileCount; ++i) {
                     nk_stroke_line(canvas, bounds.x + tWidth*i, bounds.y, bounds.x + tWidth*i, bounds.y + bounds.h, 1, lineColor);
                  }

                  for (i = 1; i < self->optYTileCount; ++i) {
                     nk_stroke_line(canvas, bounds.x, bounds.y + tHeight*i, bounds.x + bounds.w, bounds.y + tHeight*i, 1, lineColor);
                  }
               }


            }       
         }

         nk_group_end(ctx);
      }      

      nk_layout_row_push(ctx, spacer);
      nk_spacing(ctx, 1);

      nk_layout_row_push(ctx, optGroupWidth);
      struct nk_panel *grpPnl = nk_window_get_panel(ctx);

      if (nk_group_begin(ctx, "Options", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {  
         if (nk_tree_push(ctx, NK_TREE_NODE, "Encoding", NK_MAXIMIZED)) {
            if (self->imported) {
               nk_layout_row_dynamic(ctx, 20, 1);
               int choice = nk_combo(ctx, ColorOptions, 3, self->colorOption, 20, nk_vec2(100, 100));
               if (choice != self->colorOption) {
                  self->colorOption = choice;
                  self->selectedEncodeColor = 0;
                  
                  int i = 0;
                  for (i = 0; i < 4; ++i) {
                     vecResize(SNESColor)(self->encodePalette[i], _colorCountFromOption(self->colorOption), &(SNESColor){0});
                  }
               }
               Int2 encSize = textureGetSize(self->encodeTest);
               Int2 impSize = textureGetSize(self->imported);

               boolean sizeUpdate = false;

               struct nk_input *in = &ctx->input;

               //x offset
               choice = nk_propertyi(ctx, "X Offset", -7, self->optXOffset, impSize.x - 1, 1, 1.0f);
               if (choice != self->optXOffset) {
                  self->optXOffset = choice;
                  sizeUpdate = true;
               }


               //y offset
               choice = nk_propertyi(ctx, "Y Offset", -7, self->optYOffset, impSize.y - 1, 1, 1.0f);
               if (choice != self->optYOffset) {
                  self->optYOffset = choice;
                  sizeUpdate = true;
               }


               //int maxTilesX = (impSize.x - self->optXOffset) / 8 + ((impSize.x - self->optXOffset) % 8 ? 1 : 0);
               choice = nk_propertyi(ctx, "X Tiles", 0, self->optXTileCount, 100, 1, 1.0f);
               if (choice != self->optXTileCount) {
                  self->optXTileCount = choice;
                  sizeUpdate = true;
               }


               //int maxTilesY = (impSize.y - self->optYOffset) / 8 + ((impSize.y - self->optYOffset) % 8 ? 1 : 0);
               choice = nk_propertyi(ctx, "Y Tiles", 0, self->optYTileCount, 100, 1, 1.0f);
               if (choice != self->optYTileCount) {
                  self->optYTileCount = choice;
                  sizeUpdate = true;
               }

               nk_checkbox_label(ctx, "Show Encode Grid", &self->optShowGrid);
               nk_checkbox_label(ctx, "Show Color Guide", &self->optShowColorGuide);

               if (nk_button_label(ctx, "Smart-Fill Palette")) {
                  _smartFillEncodedPalette(self);
               }

               

               if(nk_selectable_label(ctx, "Tile Palette Mode", NK_TEXT_ALIGN_CENTERED|NK_TEXT_ALIGN_MIDDLE, &self->paletteTileMode)) {
                  self->optShowTilePalettes =  self->paletteTileMode;
               }

               nk_label(ctx, "Current Palette", NK_TEXT_ALIGN_LEFT);

               nk_layout_row_dynamic(ctx, 20, 4);
               self->optCurrentPalette = nk_option_label(ctx, "0", self->optCurrentPalette == 0) ? 0 : self->optCurrentPalette;
               self->optCurrentPalette = nk_option_label(ctx, "1", self->optCurrentPalette == 1) ? 1 : self->optCurrentPalette;
               self->optCurrentPalette = nk_option_label(ctx, "2", self->optCurrentPalette == 2) ? 2 : self->optCurrentPalette;
               self->optCurrentPalette = nk_option_label(ctx, "3", self->optCurrentPalette == 3) ? 3 : self->optCurrentPalette;

               if (sizeUpdate) {
                  _updateEncodeTestSize(self);
               }
            }

            nk_tree_pop(ctx);
         }
         
         if (nk_tree_push(ctx, NK_TREE_NODE, "Edit Color", NK_MINIMIZED)) {
            int pIdx = self->optCurrentPalette;
            SNESColor *selected = vecAt(SNESColor)(self->encodePalette[pIdx], self->selectedEncodeColor);
            
            float layout[3] = { 15.0f, 60.0f, 60.0f };
            nk_layout_row(ctx, NK_STATIC, 20.0f, 3, layout);

            nk_spacing(ctx, 1);
            nk_labelf(ctx, NK_TEXT_RIGHT, "Index %i", self->selectedEncodeColor);
            nk_button_color(ctx, _colorToNKColor(snesColorConverTo24Bit(*selected)));

            if (self->selectedEncodeColor > 0) {
               nk_label(ctx, "R", NK_TEXT_LEFT);
               selected->r = nk_slide_int(ctx, 0, selected->r, 31, 1);
               selected->r = (nk_byte)nk_propertyi(ctx, "", 0, selected->r, 31, 1, 1);

               nk_label(ctx, "G", NK_TEXT_LEFT);
               selected->g = nk_slide_int(ctx, 0, selected->g, 31, 1);
               selected->g = (nk_byte)nk_propertyi(ctx, "", 0, selected->g, 31, 1, 1);

               nk_label(ctx, "B", NK_TEXT_LEFT);
               selected->b = nk_slide_int(ctx, 0, selected->b, 31, 1);
               selected->b = (nk_byte)nk_propertyi(ctx, "", 0, selected->b, 31, 1, 1);
            }
            else {
               //readonly
               nk_label(ctx, "R", NK_TEXT_LEFT);
               nk_slide_int(ctx, 0, 0, 31, 1);
               nk_propertyi(ctx, "", 0, 0, 31, 1, 1);

               nk_label(ctx, "G", NK_TEXT_LEFT);
               nk_slide_int(ctx, 0, 0, 31, 1);
               nk_propertyi(ctx, "", 0, 0, 31, 1, 1);

               nk_label(ctx, "B", NK_TEXT_LEFT);
               nk_slide_int(ctx, 0, 0, 31, 1);
               nk_propertyi(ctx, "", 0, 0, 31, 1, 1);
            }

            nk_tree_pop(ctx);
         }
         
         if (nk_tree_push(ctx, NK_TREE_NODE, "Palette", NK_MINIMIZED)) {
            struct nk_panel *pnl = nk_window_get_panel(ctx);
            
            nk_layout_row_dynamic(ctx, 500.0f, 1);
            if (nk_group_begin(ctx, "palGroup", NK_WINDOW_BORDER)) {
              nk_style_push_flags(ctx, &ctx->style.button.text_alignment, NK_TEXT_ALIGN_LEFT| NK_TEXT_ALIGN_MIDDLE);               
               nk_layout_row_dynamic(ctx, 20, 1);

               nk_button_label(ctx, "Paletes!");

               nk_style_pop_flags(ctx);
               nk_group_end(ctx);
            }
            nk_tree_pop(ctx);
         }

         nk_group_end(ctx);
      }
      nk_layout_row_end(ctx);


      nk_layout_row_begin(ctx, NK_STATIC, palHeight, 5);
      nk_layout_row_push(ctx, imgGroupWidth + self->grp1Resize);
      if (nk_group_begin(ctx, "Unique Colors", 0)) {

         if (self->imported) {
            struct nk_rect bounds;
            struct nk_panel *pnl = nk_window_get_panel(ctx);

            float palWidth = 25.0f, palHeight = 25.0f;

            nk_layout_row_dynamic(ctx, palHeight, 1);

            int perRow = (int)(pnl->bounds.w / palWidth - 1);

            int rowSize = 0;
            int cIdx = 0;
            vecForEach(ColorMapEntry, c, self->importedColors, {
               if (!rowSize) {
                  nk_widget(&bounds, ctx);
                  bounds.w = palWidth;
                  bounds.h = palHeight - 5;
               }
               
               if (nk_input_is_mouse_click_in_rect(in, NK_BUTTON_LEFT, bounds)) {
                  self->selectedColorLink = cIdx;
               }

               if (nk_input_is_mouse_click_in_rect(in, NK_BUTTON_RIGHT, bounds)) {
                  if (self->selectedEncodeColor > 0) {
                     SNESColor *selCol = vecAt(SNESColor)(self->encodePalette[self->optCurrentPalette], self->selectedEncodeColor);
                     *selCol = c->color;
                  }
                  allowRightClickCancel = false;
               }

               nk_fill_rect(canvas, bounds, 0, _colorToNKColor(snesColorConverTo24Bit(c->color)));

               if (cIdx == self->selectedColorLink) {
                  nk_stroke_rect(canvas, bounds, 0, 3, nk_rgb(0, 255, 0));
               }

               bounds.x += palWidth;
               if (++rowSize >= perRow) {
                  rowSize = 0;
               }
               ++cIdx;
            });

            Int2 impSize = textureGetSize(self->imported);

            nk_labelf(ctx, NK_TEXT_ALIGN_LEFT, "Colors: %i", vecSize(ColorMapEntry)(self->importedColors));
            nk_labelf(ctx, NK_TEXT_ALIGN_LEFT, "Size: %i x %i", impSize.x, impSize.y);
         }
         
         nk_group_end(ctx);
      }
      
      nk_layout_row_push(ctx, spacer);
      nk_spacing(ctx, 1);

      nk_layout_row_push(ctx, imgGroupWidth + self->grp2Resize);
      if (nk_group_begin(ctx, "Encode Colors", 0)) {
         struct nk_rect bounds;
         struct nk_panel *pnl = nk_window_get_panel(ctx);
         float palWidth = 25.0f, palHeight = 25.0f;
         int pIdx = self->optCurrentPalette;

         nk_layout_row_dynamic(ctx, palHeight, 1);

         int perRow = (int)(pnl->bounds.w / palWidth - 1);
         int rowSize = 0;
         int cIdx = 0;
         vecForEach(SNESColor, c, self->encodePalette[pIdx], {
            if (!rowSize) {
               nk_widget(&bounds, ctx);
               bounds.w = palWidth;
               bounds.h = palHeight - 5;
            }

            if (cIdx == 0) {
               //draw the transparency square
               nk_fill_rect(canvas, bounds, 0, nk_rgb(255, 255, 255));
               struct nk_rect halfBounds = bounds;

               halfBounds.w /= 2.0f;
               halfBounds.h /= 2.0f;
               nk_fill_rect(canvas, halfBounds, 0, nk_rgb(128, 128, 128));

               halfBounds.x += halfBounds.w;
               halfBounds.y += halfBounds.h;
               nk_fill_rect(canvas, halfBounds, 0, nk_rgb(128, 128, 128));
            }
            else {
               nk_fill_rect(canvas, bounds, 0, _colorToNKColor(snesColorConverTo24Bit(*c)));
            }

            if (cIdx == self->selectedEncodeColor) {
               nk_stroke_rect(canvas, bounds, 3, 4, nk_rgb(255, 100, 100));
            }

            if (self->selectedColorLink >= 0) {
               ColorMapEntry *entry = vecAt(ColorMapEntry)(self->importedColors, self->selectedColorLink);

               if (entry->encodingIndex[self->optCurrentPalette] == cIdx) {
                  nk_stroke_rect(canvas, bounds, 0, 3, nk_rgb(0, 255, 0));
               }

               if (nk_input_is_mouse_click_in_rect(in, NK_BUTTON_LEFT, bounds)) {
                  entry->encodingIndex[self->optCurrentPalette] = cIdx;
               }
            }

            if (nk_input_is_mouse_click_in_rect(in, NK_BUTTON_LEFT, bounds)) {
               self->selectedEncodeColor = cIdx;
            }            
            
            bounds.x += palWidth;
            if (++rowSize >= perRow) {
               rowSize = 0;
            }

            ++cIdx;
         });

         nk_group_end(ctx);
      }

      nk_layout_row_push(ctx, spacer);
      nk_spacing(ctx, 1);

      nk_layout_row_push(ctx, optGroupWidth);
      if (nk_group_begin(ctx, "saveLoad", 0)) {
         static char text[9][64];
         static int text_len[9];

         float layout[] = { 0.6f, 0.4f };

         nk_layout_row(ctx, NK_DYNAMIC, 20, 2, layout);
         nk_edit_string(ctx, NK_EDIT_SIMPLE, text[0], &text_len[0], 64, nk_filter_ascii);
         nk_button_label(ctx, "Palette");

         nk_edit_string(ctx, NK_EDIT_SIMPLE, text[1], &text_len[1], 64, nk_filter_ascii);
         nk_button_label(ctx, "Char Map");

         nk_group_end(ctx);
      }

      nk_layout_row_end(ctx);

      if (allowRightClickCancel && nk_input_is_mouse_released(in, NK_BUTTON_RIGHT) && self->selectedColorLink >= 0) {
         self->selectedColorLink = -1;
      }

      if (allowRightClickCancel && nk_input_is_mouse_released(in, NK_BUTTON_RIGHT)) {
         self->paletteTileMode = false;
         self->optShowTilePalettes = false;
      }
   }
   
   nk_end(ctx);

   
   /*

   
   static boolean refreshFiles = true;
   static String *selectedFile = NULL;
   static Texture *ogTex = NULL;
   static SNESColor ogPalette[16] = { 0 };
   static SNESColor resPalette[16] = { 0 };


   if (nk_begin(ctx, c_str(selfwin->name), winRect,
      NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
   {
      static vec(StringPtr) *files = NULL;

      //if (!files || refreshFiles) {
      //   if (files) {
      //      vecDestroy(StringPtr)(files);
      //      selectedFile = NULL;
      //      files = NULL;
      //   }
      //   deviceContextListFiles("assets", DC_FILE_ALL, &files, "png");
      //   refreshFiles = false;
      //}

      int i = 0;
      struct nk_rect winBounds = nk_window_get_content_region(ctx);

      nk_layout_space_begin(ctx, NK_STATIC, 410, 64);
      //nk_layout_space_push(ctx, nk_rect(0, 0, 180, winBounds.h - 5));
      //if (nk_group_begin(ctx, "Files", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
      //   vecForEach(StringPtr, str, files, {
      //      nk_layout_row_dynamic(ctx, 20, 1);
      //   if (nk_button_label(ctx, c_str(*str))) {

      //      if (*str != selectedFile) {
      //         selectedFile = *str;

      //         TextureRequest request = {
      //            .repeatType = RepeatType_Clamp,
      //            .filterType = FilterType_Nearest,
      //            .path = stringIntern(c_str(selectedFile))
      //         };


      //         ogTex = textureManagerGetTexture(data->textureManager, request);

      //         _processImage(ogTex, ogPalette);
      //         memcpy(resPalette, ogPalette, sizeof(SNESColor) * 16);
      //      }

      //   }
      //   });
      //   nk_group_end(ctx);
      //}

      nk_layout_space_push(ctx, nk_rect(190, 0, 300, 300));
      if (nk_group_begin(ctx, "Original", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
         if (ogTex) {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 250, 1);
            nk_layout_row_push(ctx, 1.0f);

            enum nk_widget_layout_states state;
            struct nk_rect bounds;
            state = nk_widget(&bounds, ctx);
            if (state) {
               struct nk_image img = nk_image_id((int)textureGetGLHandle(ogTex));
               nk_draw_image(nk_window_get_canvas(ctx), bounds, &img, nk_rgb(255, 255, 255));
            }
            nk_layout_row_end(ctx);
         }

         nk_group_end(ctx);
      }

      nk_layout_space_push(ctx, nk_rect(190, 310, 300, 70));
      if (nk_group_begin(ctx, "Unique Colors", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
         struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

         //palette table
         const float palRowHeight = 20, palRectWidth = 17;
         nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 2));
         nk_layout_row_begin(ctx, NK_STATIC, palRowHeight, 1);

         nk_layout_row_push(ctx, palRectWidth * 16);

         enum nk_widget_layout_states state;
         struct nk_rect bounds;

         state = nk_widget(&bounds, ctx);
         if (state) {
            int x = 0;

            struct nk_rect pBounds = nk_rect(bounds.x, bounds.y, palRectWidth, bounds.h);
            pBounds.h /= 2;
            pBounds.w /= 2;

            nk_fill_rect(canvas, pBounds, 0, nk_rgb(128, 128, 128));
            pBounds.x += palRectWidth / 2; nk_fill_rect(canvas, pBounds, 0, nk_rgb(255, 255, 255));
            pBounds.x -= palRectWidth / 2; pBounds.y += palRowHeight / 2; nk_fill_rect(canvas, pBounds, 0, nk_rgb(255, 255, 255));
            pBounds.x += palRectWidth / 2; nk_fill_rect(canvas, pBounds, 0, nk_rgb(128, 128, 128));


            for (x = 1; x < 16; ++x) {
               struct nk_rect pBounds = nk_rect(bounds.x, bounds.y, palRectWidth, bounds.h);
               pBounds.x += x * palRectWidth;
               ColorRGBA c = snesColorConverTo24Bit(ogPalette[x]);
               nk_fill_rect(canvas, pBounds, 0, _colorToNKColor(c));

            }
         }

         nk_layout_row_end(ctx);
         nk_style_pop_vec2(ctx);



         nk_group_end(ctx);
      }

      nk_layout_space_push(ctx, nk_rect(500, 0, 300, 300));
      if (nk_group_begin(ctx, "Result", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

         nk_group_end(ctx);
      }

      nk_layout_space_push(ctx, nk_rect(500, 310, 300, 70));
      if (nk_group_begin(ctx, "Encoded Palette", NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

         struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

         //palette table
         const float palRowHeight = 20, palRectWidth = 17;
         nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 2));
         nk_layout_row_begin(ctx, NK_STATIC, palRowHeight, 1);


         nk_layout_row_push(ctx, palRectWidth * 16);

         enum nk_widget_layout_states state;
         struct nk_rect bounds;

         state = nk_widget(&bounds, ctx);
         if (state) {
            int x = 0;
            for (x = 0; x < 16; ++x) {

               struct nk_rect pBounds = nk_rect(bounds.x, bounds.y, palRectWidth, bounds.h);
               pBounds.x += x * palRectWidth;

               ColorRGBA c = snesColorConverTo24Bit(resPalette[x]);
               nk_fill_rect(canvas, pBounds, 0, _colorToNKColor(c));

            }
         }

         nk_layout_row_end(ctx);
         nk_style_pop_vec2(ctx);

         nk_group_end(ctx);
      }

      nk_layout_space_push(ctx, nk_rect(600, 390, 200, 20));
      if (nk_button_label(ctx, "Import")) {
         if (ogTex) {
            SNES *snes = data->snes;
            memcpy(snes->cgram.objPalettes.palette16s[0].colors, resPalette, sizeof(SNESColor) * 16);

            const ColorRGBA *pixels = textureGetPixels(ogTex);
            Int2 sz = textureGetSize(ogTex);
            int y = 0, x = 0, i = 0;

            //Char16 *character = (Char16 *)&snes->vram;
            //memset(character, 0, sizeof(*character));

            int tileX = 0, tileY = 0;
            Char16 *character = NULL;


            for (tileY = 0; tileY < 8; ++tileY) {
               for (tileX = 0; tileX < 8; ++tileX) {
                  character = (Char16 *)&snes->vram + tileY * 16 + tileX;
                  memset(character, 0, sizeof(Char16));

                  for (y = 0; y < 8; ++y) {
                     for (x = 0; x < 8; ++x) {
                        ColorRGBA c = pixels[(y + tileY * 8)*sz.x + (x + tileX * 8)];
                        if (c.a == 255) {
                           SNESColor bit15 = { c.r >> 3, c.g >> 3, c.b >> 3 };

                           byte2 raw = *(byte2*)&bit15;

                           for (i = 1; i < 16; ++i) {
                              byte2 raw2 = *(byte2*)&resPalette[i];

                              if (raw == raw2) {
                                 //character += tileY * 16;
                                 character->tiles[0].rows[y].planes[0] |= (i & 1) << x;
                                 character->tiles[0].rows[y].planes[1] |= ((i & 2) >> 1) << x;
                                 character->tiles[1].rows[y].planes[0] |= ((i & 4) >> 2) << x;
                                 character->tiles[1].rows[y].planes[1] |= ((i & 8) >> 3) << x;
                                 break;
                              }
                           }
                        }
                     }
                  }
               }
            }
         }
      }

      nk_layout_space_end(ctx);
   }

   nk_end(ctx);
   */
}

static void _updateDialog(GUIWindowPtr *win, AppData *data, vec(GUIWindowPtr) *remList) {
   GUIWindow *dlg = *win;
   struct nk_context *ctx = &dlg->parent->ctx;

   dlg->update(dlg, data);

   if (nk_window_is_hidden(ctx, c_str(dlg->name))) {
      nk_window_close(ctx, c_str(dlg->name));
      vecPushBack(GUIWindowPtr)(remList, win);
   }
}


void guiUpdate(GUI *self, AppData *data) {
   _viewerUpdate(self->viewer, data);
   _optionsUpdate(self->options, data);
   _taskBarUpdate(self->taskBar, data);
   _logSpudUpdate(self->logSpud, data);

   vec(GUIWindowPtr) *remList = vecCreate(GUIWindowPtr)(NULL);
   vecForEach(GUIWindowPtr, win, self->dialogs, {
      _updateDialog(win, data, remList);
   });

   vecForEach(GUIWindowPtr, win, remList, {
      vecRemove(GUIWindowPtr)(self->dialogs, win);
   });

   vecDestroy(GUIWindowPtr)(remList);

   //update flash timers
   if (encTestLineTimer++ % 30 == 0) {
      ++encTestLineCounter;
   }
}


