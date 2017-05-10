#include "GUI.h"
#include "Renderer.h"

#include "libutils/IncludeWindows.h"

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


typedef struct {
   GLuint fontTexture;

   GLuint vbo, vao, ebo;
   VertexAttribute *attrs;
} OGLData;

typedef FVF_Pos2_Tex2_Col4 GUIVertex;


static void _initOGLData(OGLData *self) {
   glGenBuffers(1, &self->vbo);
   glGenBuffers(1, &self->ebo);
   glGenVertexArrays(1, &self->vao);

   self->attrs = FVF_Pos2_Tex2_Col4_GetAttrs();
}

static void _destroyOGLData(OGLData *self) {

}

struct GUI_t {
   struct nk_context ctx;
   struct nk_font_atlas atlas;
   struct nk_buffer cmds;
   struct nk_draw_null_texture null;

   OGLData ogl;
};




GUI *guiCreate() {
   GUI *out = checkedCalloc(1, sizeof(GUI));
   return out;
}
void guiDestroy(GUI *self) {
   nk_font_atlas_clear(&self->atlas);
   nk_free(&self->ctx);
   checkedFree(self);
}

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


void guiInit(GUI *self) {
   nk_init_default(&self->ctx, 0);
   struct nk_font_atlas *atlas;
   _fontStashBegin(self, &atlas);
   _fontStashEnd(self);

   nk_buffer_init_default(&self->cmds);

   _initOGLData(&self->ogl);
}

static struct nk_color _colorToNKColor(ColorRGBA in) {
   return nk_rgb(in.r, in.g, in.b);
}

typedef struct {
   ColorRGBA original;
   SNESColor bit15;
   int hitCount;
}ColorEntry;

#define VectorT ColorEntry
#include "libutils/Vector_Create.h"

static void _processImage(Texture *tex, SNESColor *out) {
   const ColorRGBA *pixels = textureGetPixels(tex);
   Int2 sz = textureGetSize(tex);
   int x, y;

   memset(out, 0, sizeof(SNESColor) * 16);
   

   vec(ColorEntry) *entries = vecCreate(ColorEntry)(NULL);
   //ColorEntry newEntry = { 0 };
   //newEntry.original = (ColorRGBA){0,0,0,255};
   //newEntry.bit15 = (SNESColor) { 0, 0, 0};
   //newEntry.hitCount = 1;
   //vecPushBack(ColorEntry)(entries, &newEntry);

   for (y = 0; y < sz.y; ++y) {
      for (x = 0; x < sz.x; ++x) {
         ColorRGBA c = pixels[y*sz.x + x];
         if (c.a == 255) {
            SNESColor bit15 = {c.r >> 3, c.g >> 3, c.b >> 3};

            boolean found = false;
            vecForEach(ColorEntry, entry, entries, {
               if (entry->bit15.r == bit15.r && 
                  entry->bit15.g == bit15.g && 
                  entry->bit15.b == bit15.b) {
                  found = true;
                  ++entry->hitCount;
               }
            });

            if (!found) {
               ColorEntry newEntry = { 0 };
               newEntry.original = c;
               newEntry.bit15 = bit15;
               newEntry.hitCount = 1;
               vecPushBack(ColorEntry)(entries, &newEntry);
            }
         }
      }
   }

   for (x = 0; x < 15; ++x) {
      out[x+1] = vecAt(ColorEntry)(entries, x)->bit15;
   }

   vecDestroy(ColorEntry)(entries);
}



static void _buildPalette(struct nk_context *ctx, AppData *data) {

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

static void _buildImporter(struct nk_context *ctx, AppData *data) {
   struct nk_rect winRect = nk_rect((1280-825)/2, (720 - 470) / 2, 825, 470);
   static boolean refreshFiles = true;
   static String *selectedFile = NULL;
   static Texture *ogTex = NULL;
   static SNESColor ogPalette[16] = { 0 };
   static SNESColor resPalette[16] = { 0 };
   

   if (nk_begin(ctx, "Character Importer", winRect,
      NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
   {
      static vec(StringPtr) *files = NULL;

      if (!files || refreshFiles) {
         if (files) {
            vecDestroy(StringPtr)(files);
            selectedFile = NULL;
         }
         deviceContextListFiles("assets", DC_FILE_ALL, &files, "png");
         refreshFiles = false;
      }      

      int i = 0;
      struct nk_rect winBounds = nk_window_get_content_region(ctx);

      nk_layout_space_begin(ctx, NK_STATIC, 410, 64);
      nk_layout_space_push(ctx, nk_rect(0, 0, 180, winBounds.h - 5 ));
      if (nk_group_begin(ctx, "Files", NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {
         vecForEach(StringPtr, str, files, {
            nk_layout_row_dynamic(ctx, 20, 1);
            if (nk_button_label(ctx, c_str(*str))) {

               if (*str != selectedFile) {
                  selectedFile = *str;

                  TextureRequest request = {
                     .repeatType = RepeatType_Clamp,
                     .filterType = FilterType_Nearest,
                     .path = stringIntern(c_str(selectedFile))
                  };

                  ogTex = textureManagerGetTexture(data->textureManager, request);
                                    
                  _processImage(ogTex, ogPalette);
                  memcpy(resPalette, ogPalette, sizeof(SNESColor) * 16);
               }
               
            }
         });
         nk_group_end(ctx);
      }

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
            ColorRGBA c = snesColorConverTo24Bit(ogPalette[x]);
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
               character = (Char16 *)&snes->vram + tileY*16 + tileX;
               memset(character, 0, sizeof(Char16));

            for (y = 0;  y < 8; ++y) {
            for (x = 0;  x < 8; ++x) {
               ColorRGBA c = pixels[(y + tileY*8)*sz.x + (x + tileX*8)];
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
            }}}}
         }
      }

      nk_layout_space_end(ctx);



   }

   nk_end(ctx);
}

void guiUpdate(GUI *self, AppData *data) {
   struct nk_context *ctx = &self->ctx;

   struct nk_rect winRect = nk_rect(1024, 0, 256, 720);
   static boolean openDemo = false, openImporter = false;

   if (nk_begin(ctx, "Options", winRect,
     NK_WINDOW_MINIMIZABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE))
   {
      _buildPalette(ctx, data);

      if (nk_tree_push(ctx, NK_TREE_TAB, "Tools", NK_MINIMIZED)) {
         nk_layout_row_dynamic(ctx, 20, 1);
         if (nk_button_label(ctx, "Character Importer")) {
            openImporter = true;
            nk_window_show(ctx, "Character Importer", NK_SHOWN);
            nk_window_set_focus(ctx, "Character Importer");
         }
         nk_layout_row_dynamic(ctx, 20, 1);
         if (nk_button_label(ctx, "Nuklear Demo")) {
            openDemo = true;
            nk_window_show(ctx, "Overview", NK_SHOWN);
            nk_window_set_focus(ctx, "Overview");
         }

         nk_tree_pop(ctx);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Testing", NK_MINIMIZED)) {

         nk_layout_row_dynamic(ctx, 20, 1);
         nk_checkbox_label(ctx, "Debug Render", &data->snesRenderWhite);

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


         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);       
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Frame: %05.2f", full/1000.0f);
         nk_progress(ctx, &full, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Total frame time (ms)");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Step: %05.2f", update / 1000.0f);
         nk_progress(ctx, &update, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Frame minus fps-waits");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Game: %05.2f", gameUpdate / 1000.0f);
         nk_progress(ctx, &gameUpdate, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Game Step");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "SNES: %05.2f", snes / 1000.0f);
         nk_progress(ctx, &snes, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "SNES Software Render");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "Rend: %05.2f", render / 1000.0f);
         nk_progress(ctx, &render, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Full Render (incl GUI)");
         }

         nk_layout_row_dynamic(ctx, 15, 2);
         wBounds = nk_widget_bounds(ctx);
         nk_labelf(ctx, NK_TEXT_ALIGN_RIGHT, "GUI: %05.2f", gui / 1000.0f);
         nk_progress(ctx, &gui, 30000, nk_false);
         if (nk_input_is_mouse_hovering_rect(&ctx->input, wBounds)) {
            nk_tooltip(ctx, "Time spent in Nuklear");
         }

         nk_tree_pop(ctx);
      }




      


      nk_layout_row_dynamic(ctx, 0, 1);
      enum nk_widget_layout_states state;
      struct nk_rect bounds;
      state = nk_widget(&bounds, ctx);
      if (state) {
         static Texture *logo = NULL;

         if (!logo) {
            TextureRequest request = {
               .repeatType = RepeatType_Clamp,
               .filterType = FilterType_Nearest,
               .path = stringIntern("assets/logo.png")
            };
            logo = textureManagerGetTexture(data->textureManager, request);
         }

         struct nk_image img = nk_image_id((int)textureGetGLHandle(logo));

         bounds.h = bounds.w * (49.0f / 145.0f);
         nk_draw_image(nk_window_get_canvas(ctx), bounds, &img, nk_rgba(255, 255, 255, 128));
      }

   }
   nk_end(ctx);

   struct nk_rect viewerRect = nk_rect(0, 0, 1024, 720);
   if (nk_begin(ctx, "Viewer", viewerRect,
      NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_SCALABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE  ))
   {
      struct nk_rect winBounds = nk_window_get_content_region(ctx);
      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0, 0));


      nk_layout_row_begin(ctx, NK_DYNAMIC, winBounds.w * (168.0f / 256.0), 1);
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



   if (openImporter) {
      _buildImporter(ctx, data);
   }

   
   if (openDemo) {
      nuklear_overview(ctx);
   }
}


static void _render(GUI *self, Renderer *r) {
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

   nk_draw_foreach(cmd, &self->ctx, &self->cmds) {
      if (!cmd->elem_count) continue;
      glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);

      glScissor((GLint)(cmd->clip_rect.x),
            (GLint)((720 - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h))),
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


void guiRender(GUI *self, Renderer *r) {
   //_buildLayout(self);

   //nuklear_overview(&self->ctx);

   _render(self, r);
}