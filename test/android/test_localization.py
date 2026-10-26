"""Production catalogue/UTF-8 rendering and launcher selection regressions."""
from pathlib import Path
import subprocess,tempfile,json,os
root=Path(__file__).resolve().parents[2]
catalogue=json.loads((root/'localization/catalog.json').read_text())
# Empty save slots use the original 12-cell rolling-letter path, not UTF-8 text.
for translated in catalogue['strings'][catalogue['misc_indices']['15']].values():
 assert len(translated)<=12 and all(c==' ' or 'A'<=c<='Z' for c in translated)
flags=['-I'+str(root/p) for p in ['src/DETHRACE','src/DETHRACE/common','src/DETHRACE/pd','src/BRSRC13/include','src/harness/include','src/S3/include','src/smackw32/include']]
fixture=r'''
#include "localization.h"
#include "brender/brender.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
tDR_font gFonts[21];
static int calls;
br_pixelmap* gBack_screen;
br_font* gFont_7;
int gGraf_data_index;
static char caption[256];
br_uint_16 BrPixelmapTextWidth(br_pixelmap* pm,br_font* font,char* text) { return strlen(text)*4-1; }
void BrPixelmapText(br_pixelmap* pm,br_int_32 x,br_int_32 y,br_uint_32 colour,br_font* font,char* text) {
    assert(strlen(caption)+strlen(text)<sizeof(caption));strcat(caption,text);
}
void BrPixelmapRectangleFill(br_pixelmap* pm,br_int_32 x,br_int_32 y,br_int_32 w,br_int_32 h,br_uint_32 colour) {
    assert(x>=0&&y>=0&&x+w<=pm->width&&y+h<=pm->height);
    for(int row=y;row<y+h;row++)memset((char*)pm->pixels+row*pm->row_bytes+x,colour,w);
}
void DRPixelmapText(br_pixelmap* pm,int x,int y,tDR_font* font,char* text,int right) {
    assert((unsigned char)text[0]>=font->offset);
    assert((unsigned char)text[0]-font->offset<font->num_entries);
    assert(x+3<=right); ++calls;
    if(x>=0&&y>=0&&x<pm->width&&y<pm->height)
        ((unsigned char*)pm->pixels)[(pm->base_y+y)*pm->row_bytes+pm->base_x+x]=9;
}
int main(void) {
 char original[]="Invulnerability on";
 DRLanguageInit(NULL);assert(DRLocalize(original)==original);
 assert(DRLocalizeMisc(0,original)==original);
 DRLanguageInit("xx");assert(DRLocalize(original)==original);
 const char* languages[]={"es","it","fr"};
 const char* expected[]={"Invulnerabilidad activada","Invulnerabilità attiva","Invulnérabilité activée"};
 for(int i=0;i<3;i++){
   DRLanguageInit(languages[i]);assert(!strcmp(DRLocalizeMisc(0,original),expected[i]));
   assert(!strcmp(DRLocalize("Map"),i==0?"Mapa":i==1?"Mappa":"Carte"));
   char unknown[]="A CUSTOM CAR NAME";assert(DRLocalize(unknown)==unknown);
   assert(DRLocalizeMisc(249,unknown)==unknown);
 }
 DRLanguageInit("fr");
 unsigned char glyphs[96*8*3];memset(glyphs,9,sizeof(glyphs));
 br_pixelmap image={0};image.pixels=glyphs;image.width=3;image.height=96*8;image.row_bytes=3;
 tDR_font font={0};font.images=&image;font.offset=32;font.num_entries=96;font.height=8;font.spacing=1;
 for(int i=0;i<96;i++)font.width_table[i]=3;
 gFonts[6]=font;
 const unsigned char* p=(const unsigned char*)"éñçÀ";
 assert(DRLanguageNext(&p)==233);assert(DRLanguageNext(&p)==241);
 assert(DRLanguageNext(&p)==231);assert(DRLanguageNext(&p)==192);assert(!*p);
 assert(DRLanguageTextWidth(&font,"éñçÀ",0)==15);
 assert(DRLanguageTextWidth(&font,"\xFA" "crédits",1)==27);
 assert(DRLanguageHasUtf8("réparation"));assert(!DRLanguageHasUtf8("\xFA" "credits"));
 unsigned char guarded[64*24+32];memset(guarded,77,sizeof(guarded));
 br_pixelmap target={0};target.pixels=guarded+16;target.width=60;target.height=20;
 target.row_bytes=64;target.base_x=2;target.base_y=1;target.type=BR_PMT_INDEX_8;
 calls=0;DRLanguageDrawText(&target,0,0,&font,"éñçÀ",60,0);assert(calls==4);
 assert(guarded[16+64+2+2]==9); // acute accent is an actual palette pixel
 for(int i=0;i<16;i++)assert(guarded[i]==77);
 for(int i=64*24+16;i<64*24+32;i++)assert(guarded[i]==77);
 calls=0;DRLanguageDrawText(&target,0,0,&font,"éñçÀ",7,0);assert(calls==2);
 DRLanguageDrawText(&target,-10,-10,&font,"éñçÀ",60,0);
 // Exercise production menu overlay with original English and a translated caption.
 unsigned char ui_pixels[130*30];memset(ui_pixels,77,sizeof(ui_pixels));
 br_pixelmap ui={0};ui.pixels=ui_pixels;ui.width=130;ui.height=30;ui.row_bytes=130;ui.type=BR_PMT_INDEX_8;
 br_font menu_font={0};menu_font.glyph_y=8;gFont_7=&menu_font;gBack_screen=&ui;gGraf_data_index=1;
 tFlicette on={0},push={0};on.flic_index=335;push.flic_index=15;
 tMouse_area area={0};area.left[1]=4;area.right[1]=120;area.top[1]=4;area.bottom[1]=20;area.new_choice=0;
 tInterface_spec spec={0};spec.number_of_button_flics=spec.number_of_mouse_areas=1;
 spec.flicker_on_flics=&on;spec.pushed_flics=&push;spec.mouse_areas=&area;
 DRLanguageInit("en");DRLanguageInterface(&spec,0);assert(!caption[0]);assert(ui_pixels[5*130+5]==77);
 DRLanguageInit("fr");DRLanguageInterface(&spec,0);assert(!strcmp(caption,"Nouvelle partie"));
 assert(ui_pixels[0]==77&&ui_pixels[29*130+129]==77);assert(ui_pixels[5*130+5]==0);
 DRLanguageInit("en");assert(DRLocalize(original)==original);assert(!DRLanguageHasUtf8("é"));
 puts("PASS native languages, English fallback, accents, HUD font controls, width and clipped/subview drawing");
}
'''
java=r'''
package com.dethrace.android;
public class LanguageTest {
 public static void main(String[] args) {
  java.util.Locale.setDefault(java.util.Locale.FRENCH);
  if(!UiLanguage.text("Home").equals("Home"))throw new AssertionError("OS locale changed default");
  String[] codes={"es","it","fr"};String[] expected={"Inicio","Home","Accueil"};
  for(int i=0;i<3;i++){
   UiLanguage.set(codes[i]);if(!UiLanguage.text("Home").equals(expected[i]))throw new AssertionError();
   if(UiLanguage.text("PLAY Carmageddon  ›").equals("PLAY Carmageddon  ›"))throw new AssertionError();
   if(!UiLanguage.text("PLAY Carmageddon  ›").contains("Carmageddon"))throw new AssertionError();
   if(!UiLanguage.text("A CUSTOM NAME").equals("A CUSTOM NAME"))throw new AssertionError();
  }
  UiLanguage.set("invalid");if(!UiLanguage.text("Home").equals("Home"))throw new AssertionError();
  UiLanguage.set("en");if(!UiLanguage.text("PLAY Carmageddon  ›").equals("PLAY Carmageddon  ›"))throw new AssertionError();
  System.out.println("PASS launcher explicit selection, English default under French OS locale, template identity and fallback");
 }
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(fixture)
 subprocess.run(['gcc','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-DBR_ENDIAN_LITTLE=1']+flags+[str(root/'src/DETHRACE/common/localization.c'),str(d/'test.c'),'-Wl,--gc-sections','-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 (d/'LanguageTest.java').write_text(java)
 java_home=Path(os.environ.get('JAVA_HOME','/usr/lib/jvm/java-17-openjdk-amd64'))
 # The runtime also supports JDK compiler modules without a javac executable.
 subprocess.run([str(java_home/'bin/java'),'-m','jdk.compiler/com.sun.tools.javac.Main','-d',str(d),str(root/'android/app/src/main/java/com/dethrace/android/UiLanguage.java'),str(d/'LanguageTest.java')],check=True)
 subprocess.run([str(java_home/'bin/java'),'-cp',str(d),'com.dethrace.android.LanguageTest'],check=True)
