#include "snes.h"

#define OBJS_PER_LINE 32
#define OBJ_TILES_PER_LINE 34


ColorRGBA snesColorConverTo24Bit(SNESColor in) {
   return (ColorRGBA) {
      (in.r << 3),
      (in.g << 3),
      (in.b << 3),
      255
   };
}

static void _getSecondaryOAMData(OAM *self, byte idx, byte *x9Out, byte *szOut) {
   byte secIdx = idx / 4;
   switch (idx % 4) {
   case 0: 
      *x9Out = self->secondary[secIdx].x9_0;
      *szOut = self->secondary[secIdx].sz_0;
      break;
   case 1:
      *x9Out = self->secondary[secIdx].x9_1;
      *szOut = self->secondary[secIdx].sz_1;
      break;
   case 2:
      *x9Out = self->secondary[secIdx].x9_1;
      *szOut = self->secondary[secIdx].sz_1;
      break;
   case 3:
      *x9Out = self->secondary[secIdx].x9_1;
      *szOut = self->secondary[secIdx].sz_1;
      break;
   } 
}

//output is 512x168 32-bit color RGBA
void snesRender(SNES *self, ColorRGBA *out) {
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
      for (obj = 0; obj < 128 && obj < self->oam.objCount &&  objCount < OBJS_PER_LINE; ++obj) {
         //determine base don obj height the first 32 objs on this sl
         Sprite *spr = self->oam.primary + obj;
         byte x9 = 0, sz = 0;
         _getSecondaryOAMData(&self->oam, obj, &x9, &sz);

         if (spr->y <= y && spr->y + (objTileCountY[sz] * 8) > y) {
            slObjs[objCount++] = obj;
         }
      }

      //time, from the range build a list of at most 34 8x8 tiles
      //iterate reverse order
      for (obj = 31; obj < OBJS_PER_LINE; --obj) {
         Sprite *spr = self->oam.primary + slObjs[obj];
         byte x9 = 0, sz = 0;
         _getSecondaryOAMData(&self->oam, slObjs[obj], &x9, &sz);

         TwosComplement9 _tX = { 0 };
         _tX.twos.value = spr->x;
         _tX.twos.sign = x9;
         int16_t tX = _tX.raw;

         if (obj >= objCount) {
            continue;
         }

         if (tX > -(objTileCountY[sz] * 8) && tX < 256) {
            byte tileCount = objTileCountX[sz];
            byte t = 0;

            byte yTileOffset = 0;
            //figure out which vertical tile the scanline is in
            if (y - spr->y >= 8) {
               yTileOffset = (y - spr->y) / 8;
            }
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
               tile.y = y - spr->y - (yTileOffset * 8); // this is offset correctly to that character
                              
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
            *outc = (ColorRGBA) {0, 0, 0, 255};
            *(outc + 1) = (ColorRGBA) { 0, 0, 0, 255 };
         }
      }
   }
}
