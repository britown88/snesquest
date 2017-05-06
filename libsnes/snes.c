#include "snes.h"

//void mockRender(PPU *ppu) {
//   int x = 0, y = 0;
//   byte layer = 0, obj = 0;
//   byte2 main, sub;
//   byte pal, pri;
//
//   for (y = 0; y < 168; ++y) {
//      //scanline startup
//      //grab the right register set for this sl (hdma)
//      
//      typedef struct {
//         Char16 character;
//         byte2 x : 9, palette : 3, priority : 2;
//      }ObjTile;
//
//      byte slObjs[32];//scanline objs
//      ObjTile slTiles[34];
//
//      //range, find the first 32 sprites in the scanline
//      for (obj = 0; obj < 128; ++obj) {
//         //determine base don obj height the first 32 objs on this sl
//         ppu->oam.primary[obj];
//      }
//
//      //time, from the range build a list of at most 34 8x8 tiles
//      for (obj = 31; obj < 32; --obj) {
//         
//      }
//
//      for (x = 0; x < 256; ++x) {   
//
//         //do this first for the subpixel, then for the main pixel
//
//         //only do this is obj designated to sub
//         for (obj = 0; obj < 34; ++obj) {
//            //look at all the objTiles and see if they overalp x.
//            //if they do, get the color and break (unless color is 0, then continue)
//            //also store out the priority
//         }
//
//         //if we found a color, depending on mode and priority, we can skip bg
//
//         for (layer = 0; layer < 4; ++layer) {
//            //index directly into the appropriate tilemap
//            //the pixel grabbed here is affected by windows, mosaics, and of course mode
//         }
//
//         //Nowe we have our sub pixel color
//                       
//      }
//   }
//}