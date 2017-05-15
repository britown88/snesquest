#include "snes.h"

#define OBJS_PER_LINE 32
#define OBJ_TILES_PER_LINE 34


ColorRGBA snesColorConverTo24Bit(SNESColor in) {
   /*
   stretch top 3 bits into lower 3 bits of target
   5bit :     43210
   target: 43210432
   */

   return (ColorRGBA) {
      .r = (in.r >> 2) | (in.r << 3),
      .g = (in.g >> 2) | (in.g << 3),
      .b = (in.b >> 2) | (in.b << 3),
      .a = 255
   };
}

SNESColor snesColorConvertFrom24Bit(ColorRGBA in) {
   return (SNESColor) { .r = in.r >> 3, .g = in.g >> 3, .b = in.b >> 3 };
}

//output is 512x168 32-bit color RGBA
void snesRender(SNES *self, ColorRGBA *out, int flags) {
   int x = 0, y = 0;
   byte layer = 0, obj = 0;


   for (y = 0; y < SNES_SCANLINE_COUNT; ++y) {
      //Setup scanline
      Registers *r = &self->hdma[y];

      //determine the obj tilecounts
      byte objTileCountX[2] = { 0 };
      byte objTileCountY[2] = { 0 };
      byte objCount = 0, objTileCount = 0;

      switch (r->objSizeAndBase.objSize) {
      case 0: objTileCountX[0] = 1; objTileCountY[0] = 1; objTileCountX[1] = 2; objTileCountY[1] = 2; break;
      case 1: objTileCountX[0] = 1; objTileCountY[0] = 1; objTileCountX[1] = 4; objTileCountY[1] = 4; break;
      case 2: objTileCountX[0] = 1; objTileCountY[0] = 1; objTileCountX[1] = 8; objTileCountY[1] = 8; break;
      case 3: objTileCountX[0] = 2; objTileCountY[0] = 2; objTileCountX[1] = 4; objTileCountY[1] = 4; break;
      case 4: objTileCountX[0] = 2; objTileCountY[0] = 2; objTileCountX[1] = 8; objTileCountY[1] = 8; break;
      case 5: objTileCountX[0] = 4; objTileCountY[0] = 4; objTileCountX[1] = 8; objTileCountY[1] = 8; break;
      case 6: objTileCountX[0] = 2; objTileCountY[0] = 4; objTileCountX[1] = 4; objTileCountY[1] = 8; break;
      case 7: objTileCountX[0] = 2; objTileCountY[0] = 4; objTileCountX[1] = 4; objTileCountY[1] = 4; break;
      }

      //we can determine the adresses of the obj characters in vram
      Char16 *objChars[2];
      objChars[0] = (Char16*)((byte*)&self->vram + (r->objSizeAndBase.baseAddr << 14));
      objChars[1] = (Char16*)((byte*)objChars[0] + ((r->objSizeAndBase.baseGap + 1) << 13));

      typedef struct {
         Char16 character;
         int16_t x;
         byte y;
         byte palette : 3, priority : 2, flipX:1, flipY:1;
      }ObjTile;

      byte slObjs[OBJS_PER_LINE];//scanline objs
      ObjTile slTiles[OBJ_TILES_PER_LINE];//scanline tiles

      //range, find the first 32 sprites in the scanline
      byte secondaryIndex = 0;
      for (obj = 0; obj < 128 && obj < self->oam.objCount &&  objCount < OBJS_PER_LINE; ++obj) {
         //determine base don obj height the first 32 objs on this sl
         Sprite *spr = self->oam.primary + obj;

         byte si2 = obj & 3; //obj%4
         byte x9 = !!((*(byte*)&self->oam.secondary[secondaryIndex]) & (1 << (si2 * 2)));
         byte sz = !!((*(byte*)&self->oam.secondary[secondaryIndex]) & (1 << (si2 * 2 + 1)));         
         byte pxHeight = objTileCountY[sz] * 8;
         if (si2 == 3) { ++secondaryIndex; }

         if (
            (spr->y <= y && spr->y + pxHeight > y) || //scanline falls on sprite
            (spr->y >= 256 - pxHeight && y <  (byte)(spr->y + pxHeight))) { //
            slObjs[objCount++] = obj;
         }
      }

      //time, from the range build a list of at most 34 8x8 tiles
      //iterate reverse order
      for (obj = 31; obj < OBJS_PER_LINE; --obj) {
         Sprite *spr = self->oam.primary + slObjs[obj];  

         byte secondaryIndex = slObjs[obj] >> 2;
         byte si2 = slObjs[obj] & 3; //obj%4
         byte x9 = !!((*(byte*)&self->oam.secondary[secondaryIndex]) & (1 << (si2 * 2)));
         byte sz = !!((*(byte*)&self->oam.secondary[secondaryIndex]) & (1 << (si2 * 2 + 1)));
         byte pxHeight = objTileCountY[sz] * 8;

         TwosComplement9 _tX = { 0 };
         _tX.twos.value = spr->x;
         _tX.twos.sign = x9;
         if (_tX.twos.sign) {
            _tX.twos.unused = ~_tX.twos.unused;
         }
         int16_t tX = _tX.raw;

         if (obj >= objCount) {
            continue;
         }

         if (tX > -(objTileCountX[sz] * 8) && tX < 256) {
            byte tileCount = objTileCountX[sz];
            byte t = 0;            

            byte yTileOffset = 0;


            byte bot = spr->y + pxHeight - 1;
            //figure out which vertical tile the scanline is in
            yTileOffset = objTileCountY[sz] - 1 - ((bot - y) / 8);

            //now if its flipped we need to take the opposite
            if (spr->flipY) {
               yTileOffset = objTileCountY[sz] - 1 - yTileOffset;
            }

            //loop over all horizontal 8x8 tiles and add them
            for (t = 0; t < tileCount && objTileCount < OBJ_TILES_PER_LINE; ++t) {
               ObjTile tile = { 0 };

               //get index of character (reverse if flipped
               byte charIndex = spr->flipX ? spr->character + tileCount - 1 - t : spr->character + t;               

               //every yoffset means we have top skip toward to the next row of tiles (16 at atime)
               charIndex += yTileOffset * 16;

               //now we get to nab the characters from vram
               tile.character = *(objChars[spr->nameTable] + charIndex);
               tile.flipX = spr->flipX;
               tile.flipY = spr->flipY;
               tile.palette = spr->palette;
               tile.priority = spr->priority;
               tile.x = tX + (t*8);
               tile.y = 8 - ((bot - y) - ((bot - y) / 8) * 8) - 1;
                              
               slTiles[objTileCount++] = tile;
            }
         }
      }

      //now to render the scanline
      for (x = 0; x < SNES_SIZE_X; ++x) {
         byte objPri = 0, objPalIndex = 0, objPalette = 0;

         //see if theres an obj here
         for (obj = 0; obj < objTileCount; ++obj) {
            ObjTile *t = &slTiles[obj];
            if (x >= t->x && x < t->x + 8) {
               byte objX = x - t->x;
               byte objY = t->y;

               if (t->flipX) { objX = 7 - objX; }
               if (t->flipY) { objY = 7 - objY; }

               byte palIndex = 0;

               //construct the palette index from the bitplanes
               palIndex |= (t->character.tiles[0].rows[objY].planes[0] & (1 << objX)) >> objX;
               palIndex |= ((t->character.tiles[0].rows[objY].planes[1] & (1 << objX)) >> objX) << 1;
               palIndex |= ((t->character.tiles[1].rows[objY].planes[0] & (1 << objX)) >> objX) << 2;
               palIndex |= ((t->character.tiles[1].rows[objY].planes[1] & (1 << objX)) >> objX) << 3;

               if (palIndex && t->priority >= objPri) {
                  objPalIndex = palIndex;
                  objPri = t->priority;
                  objPalette = t->palette;
               }
            }
         }

         ColorRGBA *outc = out + (y * SNES_SCANLINE_WIDTH) + (x*2);
         if (objPalIndex) {
            SNESColor c = self->cgram.objPalettes.palette16s[objPalette].colors[objPalIndex];
            ColorRGBA color24 = snesColorConverTo24Bit(c);

            *outc = color24;
            *(outc + 1) = color24;
         }
         else {
            ColorRGBA out = flags&SNES_RENDER_DEBUG_WHITE ? (ColorRGBA) {255, 255, 255, 255} : (ColorRGBA) {0, 0, 0, 255};

            *outc = out;
            *(outc + 1) = out;
         }
      }
   }
}
