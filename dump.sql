PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE "PaletteOwners" ("id" INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE, "characterMapId" INTEGER, FOREIGN KEY ("characterMapId") REFERENCES "CharacterMaps" ("id") ON DELETE CASCADE);
CREATE TABLE "Palettes" ("id" INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE, "paletteOwnerId" INTEGER, "name" STRING, "colorCount" INTEGER, "colors" BLOB, FOREIGN KEY ("paletteOwnerId") REFERENCES "PaletteOwners" ("id") ON DELETE CASCADE);
CREATE TABLE "CharacterImportData" ("id" INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE, "characterMapId" INTEGER NOT NULL, "width" INTEGER, "height" INTEGER, "pixelData" BLOB, "offsetX" INTEGER, "offsetY" INTEGER, "tileCountX" INTEGER, "tileCountY" INTEGER, "colorMapping" BLOB, FOREIGN KEY ("characterMapId") REFERENCES "CharacterMaps" ("id") ON DELETE CASCADE);
CREATE TABLE "CharacterMaps" ("id" INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE, "name" STRING, "width" INTEGER, "height" INTEGER, "colorCount" INTEGER, "data" BLOB, "tilePaletteMap" BLOB, "encodePaletteCount" INTEGER);
CREATE TABLE "CharacterEncodePalette" ("id" INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE, "characterMapId" INTEGER, "paletteId" INTEGER, "index" INTEGER, FOREIGN KEY ("characterMapId") REFERENCES "CharacterMaps" ("id") ON DELETE CASCADE, FOREIGN KEY ("paletteId") REFERENCES "Palettes" ("id") ON DELETE CASCADE);
COMMIT;
