#pragma once
// 7x7 icons. Palette: . off, R red, W white, B blue, L light blue, G green, Y yellow, O orange, g grey
struct Icon { const char *rows[7]; };

static const Icon ICON_YOUTUBE = {{
  ".RRRRR.", "RRWRRRR", "RRWWRRR", "RRWWWRR", "RRWWRRR", "RRWRRRR", ".RRRRR.",
}};
static const Icon ICON_TEMP = {{
  "...W...", "..WRW..", "..WRW..", "..WRW..", ".WRRRW.", ".WRRRW.", "..WWW..",
}};
static const Icon ICON_DROP = {{
  "...B...", "...B...", "..BBB..", ".BBLBB.", ".BLBBB.", ".BBBBB.", "..BBB..",
}};
static const Icon ICON_BELL = {{
  "...Y...", "..YYY..", ".YYYYY.", ".YYYYY.", ".YYYYY.", "YYYYYYY", "...O...",
}};
static const Icon ICON_HEART = {{
  ".......", ".RR.RR.", "RRRRRRR", "RRRRRRR", ".RRRRR.", "..RRR..", "...R...",
}};
static const Icon ICON_CHECK = {{
  ".......", "......G", ".....GG", "G...GG.", "GG.GG..", ".GGG...", "..G....",
}};

// Weather (palette adds g = grey)
static const Icon ICON_SUN = {{
  "Y..Y..Y", ".Y...Y.", "..YYY..", "Y.YYY.Y", "..YYY..", ".Y...Y.", "Y..Y..Y",
}};
static const Icon ICON_MOON = {{
  "..LLL..", ".LL....", "LL.....", "LL.....", "LL.....", ".LL....", "..LLL..",
}};
static const Icon ICON_PARTLY = {{
  "Y.Y....", ".YYY...", "YYY.WW.", ".YWWWWW", ".WWWWWW", ".......", ".......",
}};
static const Icon ICON_CLOUD = {{
  ".......", "...WW..", ".WWWWW.", "WWWWWWW", "WWWWWWW", ".......", ".......",
}};
static const Icon ICON_RAIN = {{
  "...gg..", ".ggggg.", "ggggggg", "ggggggg", ".......", ".B.B.B.", "B.B.B..",
}};
static const Icon ICON_SNOW = {{
  "...gg..", ".ggggg.", "ggggggg", "ggggggg", ".......", ".W.W.W.", "W.W.W..",
}};
static const Icon ICON_STORM = {{
  "...gg..", ".ggggg.", "ggggggg", "ggggggg", "...Y...", "..Y....", "...Y...",
}};
static const Icon ICON_FOG = {{
  ".......", "gggggg.", ".......", ".gggggg", ".......", "gggggg.", ".......",
}};

static const Icon ICON_LIKE = {{  // thumbs up with sleeve
  "...W...", "..WW...", "W.WWWWW", "W.WWWWW", "W.WWWW.", "W.WWWW.", "W.WWW..",
}};

inline const Icon *iconByName(const String &n) {
  if (n == "youtube" || n == "yt") return &ICON_YOUTUBE;
  if (n == "temp") return &ICON_TEMP;
  if (n == "drop" || n == "humidity") return &ICON_DROP;
  if (n == "bell") return &ICON_BELL;
  if (n == "heart") return &ICON_HEART;
  if (n == "check") return &ICON_CHECK;
  if (n == "sun") return &ICON_SUN;
  if (n == "like") return &ICON_LIKE;
  if (n == "cloud") return &ICON_CLOUD;
  if (n == "rain") return &ICON_RAIN;
  return nullptr;
}
