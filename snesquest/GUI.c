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

#include "shared/CheckedMemory.h"

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#include "libsnes/snes.h"
#include "AppData.h"


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

static struct nk_color _convert15bitColorTo24(struct nk_color in) {
   struct nk_color out;
   out.a = in.a;

   out.r = (byte)((255.0f * in.r) / 31);
   out.g = (byte)((255.0f * in.g) / 31);
   out.b = (byte)((255.0f * in.b) / 31);

   return out;
}

void guiUpdate(GUI *self, AppData *data) {
   struct nk_context *ctx = &self->ctx;

   static int selectedPalette = 0;

   int i = 0, j = 0;
   static struct nk_color palette[256] = { 0 };

   struct nk_rect winRect = nk_rect(1024, 0, 256, 512);  

   if (nk_begin(ctx, "Options", winRect,
     NK_WINDOW_MOVABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_SCALABLE | NK_WINDOW_BORDER | NK_WINDOW_TITLE))
   {
      if (nk_tree_push(ctx, NK_TREE_TAB, "Palette", NK_MINIMIZED)) {
         
         static boolean firstLoad = true;

         if (firstLoad) {
            for (i = 0; i < 256; ++i) {
               Color c = data->snes->cgram.bgPalette256.colors[i];
               palette[i].r = c.r;
               palette[i].g = c.g;
               palette[i].b = c.b;
            }
            firstLoad = false;
         }
         

         nk_layout_row_begin(ctx, NK_STATIC, 20, 17);
         nk_layout_row_push(ctx, 20);
         nk_label(ctx, "", NK_TEXT_CENTERED);
         for (j = 0; j < 16; ++j) {
            nk_layout_row_push(ctx, 20);
            nk_labelf(ctx, NK_TEXT_CENTERED, "%i", j);
         }   
         nk_layout_row_end(ctx);
         
         for (j = 0; j < 16; ++j) {
            nk_layout_row_begin(ctx, NK_STATIC, 20, 17);

            nk_layout_row_push(ctx, 20);
            nk_labelf(ctx, NK_TEXT_RIGHT, "%i:", j);
            for (i = 0; i < 16; ++i) {
               boolean selected = selectedPalette == j * 16 + i;
               nk_layout_row_push(ctx, 20);

               if (selected) {
                  nk_style_push_float(ctx, &ctx->style.button.border, 2.0f);
                  nk_style_push_color(ctx, &ctx->style.button.border_color, nk_rgb(255, 255, 255));                  
               }

               if (nk_button_color(ctx, _convert15bitColorTo24(palette[j * 16 + i]) )) {
                  selectedPalette = j * 16 + i;
               }

               if (selected) {
                  
                  nk_style_pop_float(ctx);
                  nk_style_pop_color(ctx);
               }

               palette[j * 16 + i].a = 255;
            }
            nk_layout_row_end(ctx);
         }
         
         if (nk_tree_push(ctx, NK_TREE_NODE, "Edit Color", NK_MAXIMIZED)) {
            //nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
            nk_layout_row_dynamic(ctx, 20, 2);
            nk_labelf(ctx, NK_TEXT_RIGHT, "Selected: %i", selectedPalette);
            nk_button_color(ctx, _convert15bitColorTo24(palette[selectedPalette]));
            

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
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Some testing", NK_MINIMIZED)) {
         enum { EASY, HARD };
         static int op = EASY;
         static int property = 20;
         nk_layout_row_static(ctx, 30, 80, 1);
         if (nk_button_label(ctx, "button")) {
            fprintf(stdout, "button pressed\n");
         }

         nk_layout_row_dynamic(ctx, 30, 2);
         if (nk_option_label(ctx, "easy", op == EASY)) {
            op = EASY;
         }
         if (nk_option_label(ctx, "hard", op == HARD)) {
            op = HARD;
         }

         nk_layout_row_dynamic(ctx, 25, 1);
         nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

         nk_tree_pop(ctx);
      }

   }
   nk_end(ctx);

   for (i = 0; i < 256; ++i) {
      Color *c = &data->snes->cgram.bgPalette256.colors[i];

      c->r = palette[i].r;
      c->g = palette[i].g;
      c->b = palette[i].b;
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