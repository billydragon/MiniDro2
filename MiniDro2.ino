/*********************************************************************
Project Name    :   Mini Dro 2
Hard revision   :   V1.0
Soft revision   :   /
Description     :   Dro system for lathe or milling machine with 3 quadrature decoder, Oled SSD1306 display and 6 push buttons for navigation
Chip            :   STM32F103C8T6
freq uc         :   72Mhz (use 8Mhz external oscillator with PLL ) 
Compiler        :   Arduino IDE 1.8.3
Author          :   G.Pailleret, 2020 
Remark          :  ok
Hardware config : 
                    * Oled display in I2C_1 (SCL = PB6 and SDA = PB7) use STM32 I2C function
                    * Quadrature decoder X on Timer1 ( PA8 and PA9)
                    * Quadrature decoder Y on Timer3 ( PA6 and PA7)
                    * Quadrature decoder Z on Timer2 ( PA0 and PA1)
                    * Button X (X reset) on PB15
                    * Button Y (Y reset) on PB14
                    * Button Z (Z reset) on PB12
                    * Button M (M reset) on PB13
                    * Navigation :
                    *           UP(PB15)
                    *  Left     OK(PB14)      Right      CANCEL(PB13)
                    *          DOWN(PB12)
                    *
                    *
Revision        :

*********************************************************************/

#include "src/GEM/GEM_u8g2.h"
#include "src/QuadDecoder/QuadDecoder.h"
#include <EEPROM.h>


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


typedef struct
{
  boolean Inverted_X;  
  boolean Inverted_Y;
  boolean Inverted_Z;
  boolean Diameter_Mode_Y;
  int  Reso_X;
  int  Reso_Y;
  int  Reso_Z;
} sConfigDro;
const sConfigDro csConfigDefault = {false,false,false,false,512,512,512};
// Variable
sConfigDro ConfigDro;

GEMItem menuItemDirX("Direction X", ConfigDro.Inverted_X);
GEMItem menuItemDirY("Direction Y", ConfigDro.Inverted_Y);
GEMItem menuItemDirZ("Direction Z", ConfigDro.Inverted_Z);
GEMItem menuItemDiamY("Diameter Y", ConfigDro.Diameter_Mode_Y);
GEMItem menuItemResoX("Resolution X", ConfigDro.Reso_X);
GEMItem menuItemResoY("Resolution Y", ConfigDro.Reso_Y);
GEMItem menuItemResoZ("Resolution Z", ConfigDro.Reso_Z);
void ActionSaveSettingsInFlash(); // Forward declaration
GEMItem menuItemButtonSaveSettings("Save settings", ActionSaveSettingsInFlash);

void ActionDro(); // Forward declaration
GEMItem menuItemButton("Return to Dro...", ActionDro);

//Main Page Menu
GEMPage menuPageMain("Dro Menu");

//Settings Page Menu
GEMPage menuPageSettings("Main Settings"); // Settings submenu
GEMItem menuItemMainSettings("Main Settings", menuPageSettings);

boolean RelativeModeX = false;
boolean RelativeModeY = false;
boolean RelativeModeZ = false;
void UpdateRelAxe();
GEMItem menuItemRelX("Relative X", RelativeModeX,UpdateRelAxe);
GEMItem menuItemRelY("Relative Y", RelativeModeY,UpdateRelAxe);
GEMItem menuItemRelZ("Relative Z", RelativeModeZ,UpdateRelAxe);


// Create menu object of class GEM_u8g2. Supply its constructor with reference to u8g2 object we created earlier
GEM_u8g2 menu(u8g2);

//Quadrature decoder
void IT_Timer1_Overflow(); // Forward declaration
void IT_Timer2_Overflow(); // Forward declaration
void IT_Timer3_Overflow(); // Forward declaration
QuadDecoder Quad_Y(3,0xFFFF,512,false,false,IT_Timer3_Overflow); //Timer 3
QuadDecoder Quad_Z(2,0xFFFF,512,true,false,IT_Timer2_Overflow); //Timer 2
QuadDecoder Quad_X(1,0xFFFF,512,false,false,IT_Timer1_Overflow); //Timer 1
void IT_Timer1_Overflow(){Quad_X.IT_OverflowHardwareTimer();}
void IT_Timer2_Overflow(){Quad_Z.IT_OverflowHardwareTimer();}
void IT_Timer3_Overflow(){Quad_Y.IT_OverflowHardwareTimer();}



void setup() {

  // U8g2 library init. Pass pin numbers the buttons are connected to.
  // The push-buttons should be wired with pullup resistors (so the LOW means that the button is pressed)
  u8g2.begin(/*Select/OK=*/ PB14, /*Right/Next=*/ 4, /*Left/Prev=*/ 3, /*Up=*/ PB15, /*Down=*/ PB12, /*Home/Cancel=*/ PB13);
  //Restore config  
  Restore_Config();
  // Menu init, setup and draw
  menu.init();
  setupMenu();
  //menu.drawMenu(); //Start with menu screen
  ActionDro(); //Start with dro screen
}

void setupMenu() {
  // Add menu items to menu page
  menuPageMain.addMenuItem(menuItemButton);
  menuPageMain.addMenuItem(menuItemRelX);
  menuPageMain.addMenuItem(menuItemRelY);
  menuPageMain.addMenuItem(menuItemRelZ);
  //Add Sub menu Settings
  menuPageMain.addMenuItem(menuItemMainSettings); 
  menuPageSettings.addMenuItem(menuItemDirX);
  menuPageSettings.addMenuItem(menuItemResoX);
  menuPageSettings.addMenuItem(menuItemDirY);
  menuPageSettings.addMenuItem(menuItemResoY);
  menuPageSettings.addMenuItem(menuItemDirZ);
  menuPageSettings.addMenuItem(menuItemResoZ);
  menuPageSettings.addMenuItem(menuItemDiamY);
  menuPageSettings.addMenuItem(menuItemButtonSaveSettings);
  // Specify parent menu page for the Settings menu page
  menuPageSettings.setParentMenuPage(menuPageMain);
  // Add menu page to menu and set it as current
  menu.setMenuPageCurrent(menuPageMain);
}

void loop() {
  // If menu is ready to accept button press...
  if (menu.readyForKey()) {
    // ...detect key press using U8g2 library
    // and pass pressed button to menu
    menu.registerKeyPress(u8g2.getMenuEvent());
  }
}

// Setup context
void ActionDro() {
  menu.context.loop = DroContextLoop;
  menu.context.enter = DroContextEnter;
  menu.context.exit = DroContextExit;
  menu.context.allowExit = false; // Setting to false will require manual exit from the loop
  menu.context.enter();
}
// Invoked once when the button is pressed
void DroContextEnter() {
  // Clear sreen
  u8g2.clear();
}

// Invoked every loop iteration
void DroContextLoop() {
  // Detect key press manually using U8g2 library
  byte key = u8g2.getMenuEvent();
  if (key == GEM_KEY_CANCEL) {
    // Exit animation routine if GEM_KEY_CANCEL key was pressed
    menu.context.exit();
  } else 
  {
    if(key == GEM_KEY_UP)Quad_X.SetZeroActiveMode();
    if(key == GEM_KEY_DOWN)Quad_Z.SetZeroActiveMode();
    if(key == GEM_KEY_OK)Quad_Y.SetZeroActiveMode();
    DisplayDrawInformations();
  }
}
// Invoked once when the GEM_KEY_CANCEL key is pressed
void DroContextExit() 
{
  // Draw menu back on screen and clear context
  menu.reInit();
  menu.drawMenu();
  menu.clearContext();
}

void Restore_Config()
{
  //Read Config in Memory
  ReadConfigInFlash(&ConfigDro);
  //Dispatch the config
  Dispatch_Config(&ConfigDro);
}
void SaveConfigInFlash(sConfigDro *pConf)
{
  unsigned int uiCount;
  char *pt;
  EEPROM.format();
  pt = (char*)pConf; 
  for(uiCount=0;uiCount<sizeof(sConfigDro);uiCount++)
  {
    EEPROM.write(uiCount,*pt);
    pt++;  
  } 
}
void ReadConfigInFlash(sConfigDro *pConf)
{
  unsigned int uiCount;
  uint16 uiState;
  uint16 value;
  char *pt;
  uiState = EEPROM_OK;
  pt = (char*)pConf; 
  for(uiCount=0;uiCount<sizeof(sConfigDro);uiCount++)
  {
    uiState |= EEPROM.read(uiCount,&value);
    *pt = (char) value;
    pt++;  
  }
  if(uiState != EEPROM_OK)
  {
    //Problem, restore default  
    *pConf = csConfigDefault;
  } 
}
void Dispatch_Config(sConfigDro *pConf)
{
  Quad_X.SetSens( pConf->Inverted_X );  
  Quad_Y.SetSens( pConf->Inverted_Y );
  Quad_Z.SetSens( pConf->Inverted_Z );
  Quad_Y.SetDiameterMode(pConf->Diameter_Mode_Y);
  Quad_X.SetResolution(pConf->Reso_X);
  Quad_Y.SetResolution(pConf->Reso_Y);
  Quad_Z.SetResolution(pConf->Reso_Z);
}

void PrintInformationOnScreen( char* str)
{
  u8g2.clearBuffer();  
  u8g2.setCursor(0, 0);
  u8g2.print(str);
  u8g2.sendBuffer();  
} 

void ActionSaveSettingsInFlash()
{
  //Store config in memort
  SaveConfigInFlash(&ConfigDro);
  //Dispatch config to function
  Dispatch_Config(&ConfigDro); 
  //PrintInformationOnScreen("Save in flash");
  //delay(100);   
}

void DisplayDrawInformations()
{
  char buffer_x[16];
  char buffer_y[16];
  char buffer_z[16];
  sprintf(buffer_x,"%+4.3f",Quad_X.GetValue());
  sprintf(buffer_y,"%+4.3f",Quad_Y.GetValue());
  sprintf(buffer_z,"%+4.3f",Quad_Z.GetValue());
  u8g2.setColorIndex(1);
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.drawLine(0,0,0,63);
  u8g2.drawLine(0,63,127,63);
  
  u8g2.setFont(u8g2_font_t0_22_mr); // choose a suitable font
  if(Quad_X.RelativeModeActived())u8g2.setColorIndex(0);
  u8g2.drawStr(2,0,"X");
  u8g2.setColorIndex(1);  
  u8g2.drawStr(15,0,buffer_x);  // write something to the internal memory
  if(Quad_Y.RelativeModeActived())u8g2.setColorIndex(0);
  u8g2.drawStr(2,20,"Y");
  u8g2.setColorIndex(1);
  u8g2.drawStr(15,20,buffer_y);  // write something to the internal memory
  if(Quad_Z.RelativeModeActived())u8g2.setColorIndex(0);
  u8g2.drawStr(2,40,"Z");
  u8g2.setColorIndex(1);
  u8g2.drawStr(15,40,buffer_z);  // write something to the internal memory
  u8g2.sendBuffer();          // transfer internal memory to the display 
}

void UpdateRelAxe()
{  
  if( RelativeModeX == true ) Quad_X.SetRelative();
  else Quad_X.SetAbsolut();  
  if( RelativeModeY == true ) Quad_Y.SetRelative();
  else Quad_Y.SetAbsolut(); 
  if( RelativeModeZ == true ) Quad_Z.SetRelative();
  else Quad_Z.SetAbsolut();     
}
