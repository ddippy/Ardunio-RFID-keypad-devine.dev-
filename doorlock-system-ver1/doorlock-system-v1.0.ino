#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <Servo.h>
#include <Keypad_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define I2CADDR 0x20
#define SS_PIN 10
#define RST_PIN 9

#define MAX_PASSWORDS 10
#define PASS_LENGTH 6


MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;

int buzzer = 3;


const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

byte rowPins[ROWS] = {0,1,2,3};
byte colPins[COLS] = {4,5,6,7};

Keypad_I2C keypad(makeKeymap(keys),rowPins,colPins,ROWS,COLS,I2CADDR,PCF8574);


// ===== PASSWORD SYSTEM =====

String adminCode="CD000";
String deleteCode="D000";
String inputPassword="";

char passwords[MAX_PASSWORDS][PASS_LENGTH+1];
int passwordCount=0;

bool passwordMode=false;
bool adminMode=false;
bool deleteMode=false;
bool confirmDelete=false;
bool addMode=false;
bool listMode=false;

int listIndex=0;

String deleteTarget="";


// ===== BACKLIGHT =====

unsigned long lastKeyTime=0;
const unsigned long backlightTimeout=10000;


// ===== EEPROM =====

void savePasswords(){
  EEPROM.write(0,passwordCount);
  int addr=1;
  for(int i=0;i<MAX_PASSWORDS;i++){
    for(int j=0;j<PASS_LENGTH;j++){
      EEPROM.write(addr++,passwords[i][j]);
    }
  }
}


void loadPasswords(){

  passwordCount=EEPROM.read(0);
  if(passwordCount>MAX_PASSWORDS) passwordCount=0;
  int addr=1;

  for(int i=0;i<MAX_PASSWORDS;i++){
    for(int j=0;j<PASS_LENGTH;j++){
      passwords[i][j]=EEPROM.read(addr++);
    }
    passwords[i][PASS_LENGTH]='\0';
  }

  if(passwordCount==0){
    strcpy(passwords[0],"123456");
    passwordCount=1;
    savePasswords();
  }
}


// ===== SETUP =====

void setup(){
  Serial.begin(9600);
  Wire.begin();
  keypad.begin(makeKeymap(keys));
  lcd.begin(16,2);
  lcd.backlight();
  setupSound();
  lcd.print("Door Lock System");
  delay(2000);
  resetScreen();
  lastKeyTime=millis();
  pinMode(buzzer,OUTPUT);
  digitalWrite(buzzer,HIGH);
  doorServo.attach(8);
  doorServo.write(0);
  SPI.begin();
  rfid.PCD_Init();
  loadPasswords();
}

// ===== LOOP =====

void loop(){
  if(millis()-lastKeyTime>=backlightTimeout){
    lcd.noBacklight();
  }

  // ===== RFID =====

  if(rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){
    lcd.backlight();
    lastKeyTime=millis();
    String uid="";

    for(byte i=0;i<rfid.uid.size;i++){
      uid+=String(rfid.uid.uidByte[i],HEX);
    }
    uid.toUpperCase();
    Serial.print("Card UID is ");
    Serial.println(uid);
    
    if(uid=="6127463"){
      openDoor();
    }
    else{
      lcd.clear();
      lcd.print("Access Denied!");
      errorSound();
      delay(2000);
      resetScreen();
    }
    rfid.PICC_HaltA();
  }

  char key=keypad.getKey();

  if(key){
    lcd.backlight();
    lastKeyTime=millis();

    if(key=='*' && !passwordMode && !adminMode && !deleteMode && !addMode && !listMode){
      passwordMode=true;
      inputPassword="";
      lcd.clear();
      lcd.print("Enter Password");
      lcd.setCursor(0,1);
      return;
    }

    // ===== PASSWORD MODE =====

    if(passwordMode){
      if(key=='#'){
        if(inputPassword==deleteCode){
          deleteMode=true;
          passwordMode=false;
          inputPassword="";
          lcd.clear();
          beepSound();
          lcd.print("Enter Del Pass");
          lcd.setCursor(0,1);
          return;
        }

        if(inputPassword==adminCode){
          adminMode=true;
          passwordMode=false;
          inputPassword="";
          lcd.clear();
          beepSound();
          lcd.print("Admin Mode");
          delay(1000);
          lcd.clear();
          lcd.print("A:Add B:Exit");
          lcd.setCursor(0,1);
          lcd.print("#:List C:Clr");
          return;
        }

        bool correct=false;

        for(int i=0;i<passwordCount;i++){
          if(inputPassword==String(passwords[i])){
            correct=true;
            break;
          }
        }

        if(correct){
          openDoor();
        }
        else{
          lcd.clear();
          lcd.print("Incorrect Pass");
          errorSound();
          delay(2000);
        }

        passwordMode=false;
        inputPassword="";
        resetScreen();
      }

      else{
        if(key!='*' && inputPassword.length()<PASS_LENGTH){
          inputPassword+=key;
          lcd.print("*");
        }
      }
    }

    // ===== DELETE MODE =====

    else if(deleteMode){

      // Cancel
      if(key=='B'){
        deleteMode=false;
        confirmDelete=false;
        inputPassword="";
        deleteTarget="";
        resetScreen();
        return;
      }

      // Ignore A C D keys
      if(key=='A' || key=='C' || key=='D'){
        return;
      }

      if(!confirmDelete){
        if(key=='#'){
          deleteTarget=inputPassword;
          lcd.clear();
          lcd.print(deleteTarget);
          lcd.setCursor(0,1);
          beepSound();
          lcd.print("Press # to del");
          confirmDelete=true;
        }

        else if(key>='0' && key<='9'){
          if(inputPassword.length()<PASS_LENGTH){
            inputPassword+=key;
            lcd.setCursor(0,1);
            lcd.print("                ");
            lcd.setCursor(0,1);
            lcd.print(inputPassword);
          }
        }

        else if(key=='*'){
          if(inputPassword.length()>0){
            inputPassword.remove(inputPassword.length()-1);
            lcd.setCursor(0,1);
            lcd.print("                ");
            lcd.setCursor(0,1);
            lcd.print(inputPassword);
          }
        }
      }
      else{
        if(key=='#'){
          bool found=false;
          for(int i=0;i<passwordCount;i++){
            if(deleteTarget==String(passwords[i])){
              for(int j=i;j<passwordCount-1;j++){
                strcpy(passwords[j],passwords[j+1]);
              }
              passwordCount--;
              savePasswords();
              found=true;
              break;
            }
          }

          lcd.clear();
          if(found){
            beepSound();
            lcd.print("Deleted!");
          }
          else{
            beepSound();
            lcd.print("Not Found");
          }

          delay(1500);
          deleteMode=false;
          confirmDelete=false;
          inputPassword="";
          deleteTarget="";
          resetScreen();
        }
      }
    }


    // ===== ADMIN MODE =====

    else if(adminMode){
      if(key=='#'){
        adminMode=false;
        listMode=true;
        listIndex=0;
        lcd.clear();
        beepSound();
        if(passwordCount==0){
          lcd.print("No Password");
        }
        else{
          lcd.print("Pass ");
          lcd.print(listIndex+1);
          lcd.setCursor(0,1);
          lcd.print(passwords[listIndex]);
        }

        return;
      }

      if(key=='A'){
        adminMode=false;
        addMode=true;
        inputPassword="";
        lcd.clear();
        beepSound();
        lcd.print("New Pass:");
        lcd.setCursor(0,1);
        return;
      }

      if(key=='C'){
        passwordCount=0;
        savePasswords();
        lcd.clear();
        beepSound();
        lcd.print("All Deleted");
        delay(1500);
        adminMode=false;
        resetScreen();
        return;
      }

      if(key=='B'){
        adminMode=false;
        resetScreen();
        return;
      }
    }


    // ===== LIST MODE =====

    else if(listMode){
      if(key=='#'){
        listIndex++;
        if(listIndex>=passwordCount){
          listIndex=0;
        }
        lcd.clear();
        beepSound();
        lcd.print("Pass ");
        lcd.print(listIndex+1);
        lcd.setCursor(0,1);
        lcd.print(passwords[listIndex]);
      }

      if(key=='*'){
        listMode=false;
        resetScreen();
      }
    }

    // ===== ADD PASSWORD =====

    else if(addMode){
      if(key=='#'){
        if(passwordCount < MAX_PASSWORDS){
          inputPassword.toCharArray(passwords[passwordCount], PASS_LENGTH + 1);
          passwordCount++;
          savePasswords();
          lcd.clear();
          beepSound();
          lcd.print("Saved!");
        }
        else{
          lcd.clear();
          errorSound();
          lcd.print("Memory Full");
        }

        delay(1500);
        addMode=false;
        inputPassword="";
        resetScreen();
      }

      else if(key>='0' && key<='9'){
        if(inputPassword.length()<PASS_LENGTH){
          inputPassword+=key;
          lcd.print(key);
        }
      }
    }
  }
}

// ===== FUNCTIONS =====

void openDoor(){
  lcd.clear();
  lcd.print("Access Granted!");
  unlockSound();
  doorServo.write(90);
  delay(6000);
  doorServo.write(0);
  lcd.clear();
  lcd.print("Door Locked");
  delay(1000);
  resetScreen();
}

void resetScreen(){
  lcd.clear();
  lcd.print("Scan Card or");
  lcd.setCursor(0,1);
  lcd.print("Enter Password");
}

// ===== SOUNDS =====

void errorSound(){

  for(int i=0;i<3;i++){
    digitalWrite(buzzer,LOW);
    delay(120);
    digitalWrite(buzzer,HIGH);
    delay(120);
  }
}

void unlockSound(){
  tone(buzzer,523); delay(150);
  tone(buzzer,659); delay(150);
  tone(buzzer,784); delay(150);
  tone(buzzer,1047); delay(300);

  noTone(buzzer);
  digitalWrite(buzzer,HIGH);
}

void beepSound(){
  digitalWrite(buzzer,LOW);
  delay(120);
  digitalWrite(buzzer,HIGH);
  delay(120);
}

void setupSound(){
  tone(buzzer,1000); delay(120);
  noTone(buzzer); delay(80);
  tone(buzzer,1200); delay(120);
  noTone(buzzer); delay(80);
  tone(buzzer,1500); delay(250);

  noTone(buzzer);
  digitalWrite(buzzer,HIGH);
}
