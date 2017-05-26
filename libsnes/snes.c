#include "snes.h"
#include "libutils/CheckedMemory.h"
#include "libutils/Rect.h"

#define OBJS_PER_LINE 32
#define OBJ_TILES_PER_LINE 34

#define MAX_RENDER_LAYERS 12

#define CHAR16_TILES_PER_ROW 16
#define CHAR4_TILES_PER_ROW 32
#define BYTES_PER_ROW (CHAR4_TILES_PER_ROW * sizeof(Char4))
#define VRAM_SIZE 0x10000
#define MAX_SB_COUNT 16

typedef struct {
   byte sizeY : 1, sizeX : 1, baseAddr : 6;
   byte charBase : 4, mosaic : 1, tSize : 1, mainScreen : 1, subScreen : 1;
   byte2 horzOffset : 10;
   byte2 vertOffset : 10;
   byte win1Invert : 1, win1Enable : 1, win2Invert : 1, win2Enable : 1, maskLogic : 2, mainMask : 1, subMask : 1;
   byte enableColorMath : 1, colorDepth: 4, bgIdx : 2;
   byte obj : 1, priority : 2;
}ProcessBG;

/* Mode     BG depth  OPT  Priorities
1 2 3 4        Front -> Back
-=-------=-=-=-=----=---============---
0       2 2 2 2    n    3AB2ab1CD0cd
1       4 4 2      n    3AB2ab1C 0c
           * if e set: C3AB2ab1  0c
2       4 4        y    3A 2B 1a 0b
3       8 4        n    3A 2B 1a 0b
4       8 2        y    3A 2B 1a 0b
5       4 2        n    3A 2B 1a 0b
6       4          y    3A 2  1a 0
7       8          n    3  2  1a 0
7+EXTBG 8 7        n    3  2B 1a 0b*/
static void _setupBGs(Registers *r, ProcessBG *bgs, byte *bgCount) {
   int i = 0;

   //so we collate this mess of registers into a per-BG collection
   ProcessBG BGs[4] = { 0 };
   BGs[0] = (ProcessBG) {
      .bgIdx = 0,
      .sizeY = r->bgSizeAndTileBase[0].sizeY,
      .sizeX = r->bgSizeAndTileBase[0].sizeX,
      .baseAddr = r->bgSizeAndTileBase[0].baseAddr,
      .charBase = r->bgCharBase.bg1,
      .mosaic = r->mosaic.enableBG1,
      .tSize = r->bgMode.sizeBG1,
      .mainScreen = r->mainScreenDesignation.bg1,
      .subScreen = r->subScreenDesignation.bg1,
      .horzOffset = r->bgScroll[0].BG.horzOffset,
      .vertOffset = r->bgScroll[0].BG.vertOffset,
      .win1Invert = r->windowMaskSettings.win1InvertBG1,
      .win1Enable = r->windowMaskSettings.win1EnableBG1,
      .win2Invert = r->windowMaskSettings.win2InvertBG1,
      .win2Enable = r->windowMaskSettings.win2EnableBG1,
      .maskLogic = r->windowMaskLogic.bg1,
      .mainMask = r->mainScreenMasking.bg1,
      .subMask = r->subScreenMasking.bg1,
      .enableColorMath = r->colorMathControl.bg1
   };
   
   if (r->bgMode.mode <= 5) {
      BGs[1] = (ProcessBG) {
         .bgIdx = 1,
            .sizeY = r->bgSizeAndTileBase[1].sizeY,
            .sizeX = r->bgSizeAndTileBase[1].sizeX,
            .baseAddr = r->bgSizeAndTileBase[1].baseAddr,
            .charBase = r->bgCharBase.bg2,
            .mosaic = r->mosaic.enableBG2,
            .tSize = r->bgMode.sizeBG2,
            .mainScreen = r->mainScreenDesignation.bg2,
            .subScreen = r->subScreenDesignation.bg2,
            .horzOffset = r->bgScroll[1].BG.horzOffset,
            .vertOffset = r->bgScroll[1].BG.vertOffset,
            .win1Invert = r->windowMaskSettings.win1InvertBG2,
            .win1Enable = r->windowMaskSettings.win1EnableBG2,
            .win2Invert = r->windowMaskSettings.win2InvertBG2,
            .win2Enable = r->windowMaskSettings.win2EnableBG2,
            .maskLogic = r->windowMaskLogic.bg2,
            .mainMask = r->mainScreenMasking.bg2,
            .subMask = r->subScreenMasking.bg2,
            .enableColorMath = r->colorMathControl.bg2
      };
   }
   
   if (r->bgMode.mode <= 1) {
      BGs[2] = (ProcessBG) {
         .bgIdx = 2,
            .sizeY = r->bgSizeAndTileBase[2].sizeY,
            .sizeX = r->bgSizeAndTileBase[2].sizeX,
            .baseAddr = r->bgSizeAndTileBase[2].baseAddr,
            .charBase = r->bgCharBase.bg3,
            .mosaic = r->mosaic.enableBG3,
            .tSize = r->bgMode.sizeBG3,
            .mainScreen = r->mainScreenDesignation.bg3,
            .subScreen = r->subScreenDesignation.bg3,
            .horzOffset = r->bgScroll[2].BG.horzOffset,
            .vertOffset = r->bgScroll[2].BG.vertOffset,
            .win1Invert = r->windowMaskSettings.win1InvertBG3,
            .win1Enable = r->windowMaskSettings.win1EnableBG3,
            .win2Invert = r->windowMaskSettings.win2InvertBG3,
            .win2Enable = r->windowMaskSettings.win2EnableBG3,
            .maskLogic = r->windowMaskLogic.bg3,
            .mainMask = r->mainScreenMasking.bg3,
            .subMask = r->subScreenMasking.bg3,
            .enableColorMath = r->colorMathControl.bg3
      };
   }

   if (r->bgMode.mode == 0) {
      BGs[3] = (ProcessBG) {
         .bgIdx = 3,
            .sizeY = r->bgSizeAndTileBase[3].sizeY,
            .sizeX = r->bgSizeAndTileBase[3].sizeX,
            .baseAddr = r->bgSizeAndTileBase[3].baseAddr,
            .charBase = r->bgCharBase.bg4,
            .mosaic = r->mosaic.enableBG4,
            .tSize = r->bgMode.sizeBG4,
            .mainScreen = r->mainScreenDesignation.bg4,
            .subScreen = r->subScreenDesignation.bg4,
            .horzOffset = r->bgScroll[3].BG.horzOffset,
            .vertOffset = r->bgScroll[3].BG.vertOffset,
            .win1Invert = r->windowMaskSettings.win1InvertBG4,
            .win1Enable = r->windowMaskSettings.win1EnableBG4,
            .win2Invert = r->windowMaskSettings.win2InvertBG4,
            .win2Enable = r->windowMaskSettings.win2EnableBG4,
            .maskLogic = r->windowMaskLogic.bg4,
            .mainMask = r->mainScreenMasking.bg4,
            .subMask = r->subScreenMasking.bg4,
            .enableColorMath = r->colorMathControl.bg4
      };
   }

   //then we build our render list in order of priority
   switch (r->bgMode.mode) {
   case 0:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // A
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // B
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = BGs[0]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // a
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // b
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[2]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // C
      bgs[i] = BGs[3]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // D
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      bgs[i] = BGs[2]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // c
      bgs[i] = BGs[3]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // d      
      break;
   case 1:
      if (r->bgMode.m1bg3pri) 
      //bgs[i] = BGs[2]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // C
      
      //bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // A
      //bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // B
      //bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      //bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // a
      //bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // b
      //bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      //if (!r->bgMode.m1bg3pri) 
      //bgs[i] = BGs[2]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // C
      //
      //bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      //bgs[i] = BGs[2]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // c
      break;
   case 2:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // A
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // B
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // a
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // b
      break;
   case 3:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 8; bgs[i].priority = 1; ++i; // A
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // B
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[0]; bgs[i].colorDepth = 8; bgs[i].priority = 0; ++i; // a
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      bgs[i] = BGs[1]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // b
      break;
   case 4:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 8; bgs[i].priority = 1; ++i; // A
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // B
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[0]; bgs[i].colorDepth = 8; bgs[i].priority = 0; ++i; // a
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // b
      break;
   case 5:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // A
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 1; ++i; // B
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // a
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      bgs[i] = BGs[1]; bgs[i].colorDepth = 2; bgs[i].priority = 0; ++i; // b
      break;
   case 6:
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 3 }; ++i;            // 3
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 1; ++i; // A
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 2 }; ++i;            // 2
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 1 }; ++i;            // 1
      bgs[i] = BGs[0]; bgs[i].colorDepth = 4; bgs[i].priority = 0; ++i; // a
      bgs[i] = (ProcessBG) { .obj = 1, .priority = 0 }; ++i;            // 0
      break;
   }

   *bgCount = i;
}


//output is 512x168 32-bit color RGBA
void snesRender(SNES *self, ColorRGBA *out, int flags) {
   int x = 0, y = 0;
   byte layer = 0, obj = 0;

   for (y = 0; y < SNES_SCANLINE_COUNT; ++y) {
      //Setup scanline
      Registers *r = &self->reg;

      int mosaicY = y % (self->reg.mosaic.size + 1);

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

      //setup our render list, list is in order of front of screen to back
      //if .obj then we can use the pixel it found for an obj
      // we'll iterate over this list twice, once for mainscreen, once for subscreen
      ProcessBG layers[MAX_RENDER_LAYERS] = { 0 };
      byte layerCount = 0;
      TileMap *tMaps[MAX_RENDER_LAYERS];
      Char4 *cMaps[MAX_RENDER_LAYERS];

      _setupBGs(r, layers, &layerCount);
      for (layer = 0; layer < layerCount; ++layer) {
         if (!layers[layer].obj) {
            tMaps[layer] = (TileMap*)((byte*)&self->vram + (layers[layer].baseAddr << 11));
            cMaps[layer] = (Char4*)((byte*)&self->vram + (layers[layer].charBase << 13));
         }
      }

      //now to render the scanline
      for (x = 0; x < SNES_SIZE_X; ++x) {
         int mosaicX = x % (self->reg.mosaic.size + 1);

         //start by determining the OBJ Pixel from the scanline obj tile data we gathered
         boolean objPresent = false;
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
               palIndex = 
                  ((t->character.tiles[0].rows[objY].planes[0] & (1 << objX)) >> objX) |
                  (((t->character.tiles[0].rows[objY].planes[1] & (1 << objX)) >> objX) << 1) |
                  (((t->character.tiles[1].rows[objY].planes[0] & (1 << objX)) >> objX) << 2) |
                  (((t->character.tiles[1].rows[objY].planes[1] & (1 << objX)) >> objX) << 3);

               if (palIndex && t->priority >= objPri) {
                  objPalIndex = palIndex;
                  objPri = t->priority;
                  objPalette = t->palette;
                  objPresent = true;
               }
            }
         }

         //define our result form, we ned to know once we're done:
         //  1. The main screen palette index (0-255 for all of cgram)
         //  2. Same for sub screen
         //  3. color math logic (enabled, halved, add/sub)

         struct {
            byte mainPIdx;
            byte subPIdx;
            byte doColorMath : 1, halve : 1, addSubtract : 1;
         } result = { 0 };


         for (layer = 0; layer < layerCount; ++layer) {
            ProcessBG *l = layers + layer;
            int bgX = x, bgY = y;//makes sense here to work off copies inc ase we need to change them (mosaics)

            bgX -= mosaicX;
            bgY -= mosaicY;

            bgX += l->horzOffset;
            bgY += l->vertOffset;

            // expecting an OBJ here, if the OBJ we found fits the priority description 
            // we can check it for clipping and use it
            if (l->obj) {
               if (objPresent && objPri == l->priority) {
                  result.mainPIdx = 128 + (objPalette * 16) + objPalIndex; //magic
                  break;
               }
               else {
                  //obj layer skip the rest
                  continue;
               }
               
            }

            //ok now to process a tilemap
            //first we need to figure out what tile to use, which depends on the width of the tile in pixels
            byte tileX = (byte)(bgX >> (l->tSize ? 4 : 3)); //divided by 8 or 16
            byte tileY = (byte)(bgY >> (l->tSize ? 4 : 3));            

            //depending on how many tile maps are given to the BG, either point at a different map or wrap around
            TileMap *tMap = tMaps[layer];
            tileX &= l->sizeX ? 63 : 31;
            if (tileX >= 32) {
               tileX &= 31; tMap += 1;
            }
            tileY &= l->sizeY ? 63 : 31;
            if (tileY >= 32) {
               tileY &= 31;  tMap += l->sizeX ? 2 : 1;
            }

            //now detemmine which tile we're using
            Tile *t = tMap->tiles + (tileY * 32 + tileX);
            if (t->tile.priority != l->priority) {
               //only draw from this tile if its set to the currenlty-drawn priority
               continue;
            }

            if (l->colorDepth == 2) {
               //we know the tile and the position within it, now we need to know the character
               Char4 *c = ((Char4*)cMaps[layer]) + t->tile.character;
               byte inTileX = (byte)(bgX & (l->tSize ? 15 : 7)); //mod16 or mod8
               byte inTileY = (byte)(bgY & (l->tSize ? 15 : 7));

               byte palIndex = palIndex =
                  !!(c->rows[inTileY].planes[0] & (1 << inTileX)) |
                  (!!(c->rows[inTileY].planes[1] & (1 << inTileX)) << 1);

               if (palIndex) {
                  result.mainPIdx = (t->tile.palette * 16) + palIndex;
                  break;
               }
            }
            else {
               //we know the tile and the position within it, now we need to know the character
               Char16 *c = ((Char16*)cMaps[layer]) + t->tile.character;
               

               byte inTileX = (byte)(bgX & (l->tSize ? 15 : 7)); //mod16 or mod8
               byte inTileY = (byte)(bgY & (l->tSize ? 15 : 7));

               if (l->tSize) {
                  if (inTileX >= 8) {
                     inTileX -= 8;
                     ++c;
                  }
                  if (inTileY >= 8) {
                     inTileY -= 8;
                     c += 16;
                  }
               }

               byte palIndex = palIndex =
                  !!(c->tiles[0].rows[inTileY].planes[0] & (1 << inTileX)) |
                  (!!(c->tiles[0].rows[inTileY].planes[1] & (1 << inTileX)) << 1) |
                  (!!(c->tiles[1].rows[inTileY].planes[0] & (1 << inTileX)) << 2) |
                  (!!(c->tiles[1].rows[inTileY].planes[1] & (1 << inTileX)) << 3);

               if (palIndex) {
                  result.mainPIdx = (t->tile.palette * 16) + palIndex;
                  break;
               }
            }

            
            
         }


         ColorRGBA *outc = out + (y * SNES_SCANLINE_WIDTH) + (x*2);
         if (result.mainPIdx) {
            SNESColor c = self->cgram.colors[result.mainPIdx];
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

typedef struct MapNode MapNode;

typedef struct {  
   Recti r;
   Char4 *data;
   MapNode *node;
}CMapSubblock;

struct CMapBlock {
   CMap *parent;
   byte colorDepth;
   byte2 width, height;
   byte sizeX, sizeY;
   CMapSubblock sb[MAX_SB_COUNT];
   byte sbCount;
};
typedef CMapBlock *CMapBlockPtr;

#define VectorT CMapBlockPtr
#include "libutils/Vector_Create.h"

static void _cMapBlockDestroy(CMapBlockPtr *self) {
   byte i = 0;
   for (i = 0; i < (*self)->sbCount; ++i) {
      checkedFree((*self)->sb[i].data);
   }
   checkedFree(*self);
}

struct MapNode {
   MapNode *child[2];
   Recti rect;
   CMapSubblock *block;
};

static void _mapNodeDestroy(MapNode *self) {
   if (self->child[0]) {
      _mapNodeDestroy(self->child[0]);
      checkedFree(self->child[0]);
   }
   if (self->child[1]) {
      _mapNodeDestroy(self->child[1]);
      checkedFree(self->child[1]);
   }
}

struct CMap {
   VRAM *parent;
   byte baseAddr;
   byte rowOffset;
   byte rowCount;

   MapNode root;
   vec(CMapBlockPtr) *blocks;
};

CMap *cMapCreate(VRAM *vram, byte baseAddr, byte rowOffset, byte rowCount) {
   size_t addr = baseAddr << 13;
   assert(addr + (BYTES_PER_ROW * (rowOffset + rowCount)) <= VRAM_SIZE && "Attempting to create character map outside of vram bounds.");

   CMap *out = checkedCalloc(1, sizeof(CMap));
   out->parent = vram;
   out->baseAddr = baseAddr;
   out->rowOffset = rowOffset;
   out->rowCount = rowCount;
   out->blocks = vecCreate(CMapBlockPtr)(&_cMapBlockDestroy);
   out->root.rect = (Recti) {0, 0, CHAR4_TILES_PER_ROW, rowCount};

   return out;
}

void cMapDestroy(CMap *self) {
   _mapNodeDestroy(&self->root);
   vecDestroy(CMapBlockPtr)(self->blocks);
   checkedFree(self);
}

static MapNode *_nodeInsert(MapNode *self, CMapSubblock *block) {
   
   //if not a leaf
   if (self->child[0]) {

      //try inserting into first child
      MapNode *newNode = _nodeInsert(self->child[0], block);
      if (newNode) {
         return newNode;
      }

      //no room, insert into second child
      return _nodeInsert(self->child[1], block);
   }
   else {
      //if theres already a block here, return
      if (self->block) {
         return NULL;
      }

      //if this isnt big enough, return 
      if (block->r.w > self->rect.w || block->r.h > self->rect.h) {
         return NULL;
      }

      //if this rect is perfectly sized, accept it
      if (block->r.w == self->rect.w && block->r.h == self->rect.h) {
         return self;
      }

      //ok at this point we need to split this node
      self->child[0] = checkedCalloc(1, sizeof(MapNode));
      self->child[1] = checkedCalloc(1, sizeof(MapNode));

      byte2 dw = self->rect.w - block->r.w;
      byte2 dh = self->rect.h - block->r.h;

      if (dw > dh) {
         self->child[0]->rect = (Recti) { self->rect.x,  self->rect.y, block->r.w, self->rect.h};
         self->child[1]->rect = (Recti) { self->rect.x + block->r.w, self->rect.y, self->rect.w - block->r.w, self->rect.h};
      }
      else {
         self->child[0]->rect = (Recti) { self->rect.x, self->rect.y, self->rect.w, block->r.h };
         self->child[1]->rect = (Recti) { self->rect.x, self->rect.y + block->r.h, self->rect.w, self->rect.h - block->r.h };
      }

      return _nodeInsert(self->child[0], block);
   }
}

CMapBlock *cMapAlloc(CMap *cmap, byte colorDepth, byte2 width, byte2 height, byte tileWidth, byte tileHeight) {
   CMapBlock *out = checkedCalloc(1, sizeof(CMapBlock));
   out->parent = cmap;
   out->colorDepth = colorDepth;
   out->width = width;
   out->height = height;
   out->sizeX = tileWidth;
   out->sizeY = tileHeight;

                     // width/8 * depth/2
   byte char4Width = (tileWidth >> 3) * (colorDepth >> 1); //the number of char4s in one tile
   byte char4Height = tileHeight >> 3;

   // most characters we can fit in one row is 32 char4's
   // split the block into as many subblocks as it needs
   out->sbCount = ((out->width * char4Width) / CHAR4_TILES_PER_ROW) + ((out->width * char4Width) % CHAR4_TILES_PER_ROW > 0 ? 1 : 0);
   assert(out->sbCount <= MAX_SB_COUNT && "Block too large");

   byte i = 0;
   for (i = 0; i < out->sbCount - 1; ++i) {
      out->sb[i].r = (Recti) { .x = 0, .y = 0, .w = CHAR4_TILES_PER_ROW, .h = height * char4Height };
      out->sb[i].data = checkedCalloc(1, sizeof(Char4) * out->sb[i].r.w * out->sb[i].r.h);
   }

   out->sb[i].r = (Recti) { .x = 0, .y = 0, .w = (out->width * char4Width) % CHAR4_TILES_PER_ROW, .h = height * char4Height };
   if (!out->sb[i].r.w) { out->sb[i].r.w = CHAR4_TILES_PER_ROW; }
   out->sb[i].data = checkedCalloc(1, sizeof(Char4) * out->sb[i].r.w * out->sb[i].r.h);

   for (i = 0; i < out->sbCount; ++i) {
      MapNode *n = _nodeInsert(&cmap->root, &out->sb[i]);
      if (n) {
         n->block = &out->sb[i];
         n->block->r = n->rect;
         n->block->node = n;
      }
      else {
         _cMapBlockDestroy(&out);
         assert(false && "Out of room, cant add block");
         return NULL;
      }
   }

   vecPushBack(CMapBlockPtr)(cmap->blocks, &out);
   return out;
}

void cMapFree(CMap *cmap, CMapBlock *block) {
   byte i = 0;
   for (i = 0; i < block->sbCount; ++i) {
      if (block->sb[i].node) {
         block->sb[i].node->block = NULL;
      }
   }
   vecRemove(CMapBlockPtr)(cmap->blocks, &block);
}

void cMapBlockSetCharacters(CMapBlock *block, Char4 *data) {
   byte char4Width = (block->sizeX >> 3) * (block->colorDepth >> 1); //the number of char4s in one tile
   byte char4Height = block->sizeY >> 3;

   byte i = 0;
   for (i = 0; i < block->sbCount; ++i) {
      byte2 y = 0;
      for (y = 0; y < block->sb[i].r.h; ++y) {
         Char4 *destAddr = block->sb[i].data + (block->sb[i].r.w * y);
         Char4 *srcAddr = data + (block->width * char4Width * y) + (CHAR4_TILES_PER_ROW * i);
         memcpy(destAddr, srcAddr, sizeof(Char4) * block->sb[i].r.w);
      }
   }
}

byte2 cMapBlockGetCharacter(CMapBlock *block, byte2 x, byte2 y) {
   //heres the fucked part we need to determine what char4 character index to use
   //based on x, y, colordepth, tilesize, and subblock count
   byte char4Width = (block->sizeX >> 3) * (block->colorDepth >> 1); //the number of char4s in one tile
   byte char4Height = block->sizeY >> 3;
   byte sbIdx = (x * char4Width) >> 5;

   CMapSubblock *sb = &block->sb[sbIdx];

   byte2 out = block->parent->rowOffset * (CHAR4_TILES_PER_ROW / char4Width);
   out += (sb->r.y + y) * (CHAR4_TILES_PER_ROW/ char4Width);
   out += sb->r.x / char4Width;
   out += x- (sbIdx * (CHAR4_TILES_PER_ROW/ char4Width));
   return out;
}

static void _commitBlock(CMapBlock *block) {
   byte char4Width = (block->sizeX >> 3) * (block->colorDepth >> 1); //the number of char4s in one tile
   byte char4Height = block->sizeY >> 3;

   VRAM *vram = block->parent->parent;
   Char4 *dest = ((byte*)vram) + (block->parent->baseAddr << 13);
   dest += block->parent->rowOffset * CHAR4_TILES_PER_ROW;

   byte i = 0;
   for (i = 0; i < block->sbCount; ++i) {
      CMapSubblock *sb = &block->sb[i];
      byte2 y = 0;
      for (y = 0; y < sb->r.h; ++y) {
         Char4 *destAddr = dest;
         destAddr += (sb->r.y + (y * char4Height)) * CHAR4_TILES_PER_ROW;
         destAddr += sb->r.x;

         Char4 *srcAddr = sb->data + (sb->r.w * y);
         memcpy(destAddr, srcAddr, sizeof(Char4) * sb->r.w);
      }
   }
}

void cMapCommit(CMap *self) {
   vecForEach(CMapBlockPtr, block, self->blocks, {
      _commitBlock(*block);
   });
}