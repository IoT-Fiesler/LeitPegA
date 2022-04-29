


/*
  Versionsnummern:
  V1 - nur Taster, kein Display, nur LED-Anzeige
  V2 - Taster, LEDs und Display mit wechselnder Anzeige, Messung nur bei gedrücktem Taster
  V3 - Taster, WiFi (UDP) via ESP auf UART3, LEDs und Display wie bei V2
 
*/


/*Pinbelegung

  Pins A0-A9: Elektroden Fuellstandsmesser
  Pin 20: Taster (Interrupt) fuer manuelles Messen
  Pins l8&19: UART 1 mit Interrupt für Kommunikation mit ESP-01
  Pin 5: Relais
  Pin 6: Ventil (via Solidstate-Relais)
  Pin A10: KTY81-210 Temperatursensor
  Pins 39-41: Display


*/

//Bibliotheken
#include <Arduino.h>
#include <SPI.h>
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>


int p = 0; // Zaehlvariabel
int P = 0; // Pegel in Prozent
int T;  // Temperatur
int Offset = 5; // Korrektur KTY81-210

//Variables and constants for Densitron LCD display

int CS = 39;
int DATA = 40;
int WR = 41;

// Pins fuer Relais
#define RELAIS 5 // Pin 5 fuer Relais
#define VENTIL 6 // Pin 6 fuer Relais fuer Ventil falls P <= 10%

// Pin fuer Taster
#define TASTER 20 // Pin 20 (Interrupt) fuer Taster

// Pins fuer Fuellstandshöhe - 10 Kanaele
#define BRAUN_10 22 //Pin 22 fuer braunen Draht & 10% Fuellung
#define WEISS_BRAUN_20 23 //Pin 23 fuer weißen (bei braun) Draht und 20% Fuellung
#define GELB_30 24 //Pin 24 fuer  gelben Draht & 30% Fuellung
#define WEISS_GELB_40 25 // Pin 25 fuer weißen (bei gelb) Draht & 40% Fuellung
#define SCHWARZ_50 26 // Pin 26 fuer schwarzen Draht & 50% Fuellung
#define WEISS_SCHWARZ_60 27 // Pin 27 fuer weißen (bei schwarz) Draht & 60% Fuellung
#define BLAU_ROT_70 28 // Pin 28 fuer blauen (bei rot) Draht & 70% Fuellung
#define ROT_80 29 // Pin 29 fuer roten Draht & 80% Fuellung
#define BLAU_WEISS_90 30 // Pin 30 fuer blauen (bei weiß) Draht & 90% Fuellung
#define WEISS_100 31 // Pinv 31 fuer weißen Draht & 100% Fuellung


//Pins 11-13 fuer SPI bzw. 50-52

#define CS_low()   digitalWrite(CS, LOW); delayMicroseconds(1)
#define CS_high()  delayMicroseconds(1); digitalWrite(CS, HIGH)
#define DATA(x)    if ((x)==0) digitalWrite(DATA, LOW); else digitalWrite(DATA, HIGH)
#define WR_pulse() digitalWrite(WR, LOW); delayMicroseconds(3); digitalWrite(WR, HIGH); delayMicroseconds(3)
//#define LED_on()   PORTB |= 0x20
//#define LED_off()  PORTB &= ~0x20

/* Definitions for the Densitron PC-6749 LCD glass (Pollin 120818)
   this is a 3-digit 16-segment LC display controlled by a HT1621

     glass segment mapping
   from the display datasheet  |  and how we use it

   ---------------------------
        | COM0 COM1 COM2 COM3
   -----+---------------------
   SEG0 | 1G   1K   1F   1E    \ lcd_buf[0] bit 0  1  2  3
   SEG1 | 1H   1L   1N   1D    /                4  5  6  7

   SEG2 | 1I   1M   1O   1P    \ lcd_buf[1] bit 0  1  2  3
   SEG3 | 1A   1J   1B   1C    /                4  5  6  7

   SEG4 |       X    Y    Z    \ lcd_buf[2] bit 0  1  2  3
   SEG5 | 2G   2K   2F   2E    /                4  5  6  7

   SEG6 | 2H   2L   2N   2D    \ lcd_buf[3] bit 0  1  2  3
   SEG7 | 2I   2M   2O   2P    /                4  5  6  7

   SEG8 | 2A   2J   2B   2C    \ lcd_buf[4] bit 0  1  2  3
   SEG9 | 3G   3K   3F   3E    /                4  5  6  7

   SEG10| 3H   3L   3N   3D    \ lcd_buf[5] bit 0  1  2  3
   SEG11| 3I   3M   3O   3P    /                4  5  6  7

   SEG12| 3A   3K   3B   3C    } lcd_buf[6] bit 0  1  2  3
   ---------------------------
*/

/*
   segment mapping table [digit][segment]
   entry is lcd_buf address in high nibble
   and bit number in low nibble
*/

const uint8_t segmap[3][16] =
{
  { 0x14, 0x16, 0x17, 0x07, 0x03, 0x02, 0x00, 0x04,
    0x10, 0x15, 0x01, 0x05, 0x11, 0x06, 0x12, 0x13
  },
  { 0x40, 0x42, 0x43, 0x33, 0x27, 0x26, 0x24, 0x30,
    0x34, 0x41, 0x25, 0x31, 0x35, 0x32, 0x36, 0x37
  },
  { 0x60, 0x62, 0x63, 0x53, 0x47, 0x46, 0x44, 0x50,
    0x54, 0x61, 0x45, 0x51, 0x55, 0x52, 0x56, 0x57
  }
};

/*
   font for 16-segment display
   it is sparse, hence organized as list
*/

const struct {
  char c;     /* the character */
  uint16_t s; /* 16 bits for segments A..P */
}  seg16font[] =
{ /* ABCDEFGHIJKLMNOP */
  { '1', 0b0110000001000000 },
  { '2', 0b1101110000111000 },
  { '3', 0b1111100000111000 },
  { '4', 0b0000001010111010 },
  { '5', 0b1011101000111000 },
  { '6', 0b1011111000111000 },
  { '7', 0b1000000001111010 },
  { '8', 0b1111111000111000 },
  { '9', 0b1111101000111000 },
  { '0', 0b1111111001010100 },
  { '+', 0b0000000010111010 },
  { '-', 0b0000000000111000 },
  { '*', 0b0000000111111111 },
  { '/', 0b0000000001010100 },
  { '.', 0b0000100000000000 },
  { '(', 0b0000000001000001 },
  { '$', 0b1011101010111010 },
  { ')', 0b0000000100000100 },
  { 'A', 0b1110011000111000 },
  { 'B', 0b1111100010011010 },
  { 'C', 0b1001111000000000 },
  { 'D', 0b1111100010010010 },
  { 'E', 0b1001111000110000 },
  { 'F', 0b1000011000110000 },
  { 'G', 0b1011111000001000 },
  { 'H', 0b0110011000111000 },
  { 'I', 0b1001100010010010 },
  { 'J', 0b1000100010010010 },
  { 'K', 0b0000000011010011 },
  { 'L', 0b0001111000000000 },
  { 'M', 0b0110011101010000 },
  { 'N', 0b0110011100010001 },
  { 'O', 0b1111111000000000 },
  { 'P', 0b1100011000111000 },
  { 'Q', 0b1111111000000001 },
  { 'R', 0b1100011000110001 },
  { 'S', 0b1011101000111000 },
  { 'T', 0b1000000010010010 },
  { 'U', 0b0111111000000000 },
  { 'V', 0b0000011001010100 },
  { 'W', 0b0110011000010101 },
  { 'X', 0b0000000101010101 },
  { 'Y', 0b0000000101010010 },
  { 'Z', 0b1001100001010100 },
  { '{', 0b0000000001011001 },
  { '_', 0b0001100000000000 },
  { '}', 0b0000000100110100 },
  { 'b', 0b0011111000111000 },
  { 'c', 0b0001110000111000 },
  { 'd', 0b0111110000111000 },
  { 'h', 0b0010011000111000 },
  { 'l', 0b0000111000000000 },
  { 'm', 0b0010010000101010 },
  { 'n', 0b0010010000111000 },
  { 'o', 0b0011110000111000 },
  { 'r', 0b0000010000110000 },
  { 't', 0b0001000010111010 },
  { 'u', 0b0011110000000000 },
  { 'v', 0b0000010000000100 },
  { 'w', 0b0010010000000101 },
  { '[', 0b1001111000000000 },
  { '|', 0b0000000010010010 },
  { ']', 0b1111100000000000 },
  { ' ', 0b0000000000000000 },
  /* ABCDEFGHIJKLMNOP */
  { 0, 0xFFFF }
};

/*
   search character in font, return segments
   nonexisting character yields empty pattern
*/
uint16_t char2seg(char c)
{
  for (uint8_t j = 0; seg16font[j].c; j++) {
    if (seg16font[j].c == c) {
      return seg16font[j].s;
    }
  }
  return 0;
}


/*
   Send a command to the LCD controller
   ! we send the command code MSB first !
    so it matches the datasheet notation
*/
void lcd_cmd(uint8_t cmd) {
  CS_low();
  DATA(1); WR_pulse();
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();
  uint8_t mask = 0x80;
  while (mask) {
    DATA(cmd & mask);
    WR_pulse();
    mask >>= 1;
  }
  DATA(1); WR_pulse();
  CS_high();
}

/*
   initialize and enable the LCD controller
*/
void lcd_init(void) {
  lcd_cmd(0b00000001); // system enable
  lcd_cmd(0b00101011); // 1/3 Bias, 4 commons
  lcd_cmd(0b00000011); // turn on LCD
}

/*
   the LCD buffer
*/
#define LCD_BUF_SIZE 7
uint8_t lcd_buf[LCD_BUF_SIZE];

/*
   clear LCD buffer
*/
void lcd_clear_buf(void)
{
  for (uint8_t i = 0; i < LCD_BUF_SIZE; i++) {
    lcd_buf[i] = 0;
  }
}

/*
   set segment bits from 'seg' for digit at position 'pos'
*/
void lcd_update_buf(uint8_t pos, uint16_t seg)
{
  uint8_t segno = 0;
  uint16_t mask = 0x8000;
  for ( ; segno < 16; segno++, mask >>= 1) {
    if (seg & mask) {
      uint8_t code = segmap[pos][segno];
      lcd_buf[code >> 4] |= 1 << (code & 7);
    }
  }
}

/*
   flush LCD buffer to the LCD controller
*/
void lcd_flush_buf(void) {
  CS_low();
  DATA(1); WR_pulse();
  DATA(0); WR_pulse();
  DATA(1); WR_pulse();

  /* address is always 0 (start of buffer) */
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();
  DATA(0); WR_pulse();

  /* bulk write the complete buffer */
  for (uint8_t i = 0; i < LCD_BUF_SIZE; i++) {
    uint8_t mask = 0x01;
    while (mask) {
      DATA(lcd_buf[i] & mask);
      WR_pulse();
      mask <<= 1;
    }
  }
  DATA(1);
  CS_high();
}


char ssid[] = "YOUR_SSID";            // your network SSID (name)
char pass[] = "YOUR_PASS";        // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status

IPAddress ip(192, 168, 178, 91);

IPAddress RecipientIP(192, 168, 178, 56);

unsigned int RecipientPort = 9292;

unsigned int localPort = 8282;  // local port to listen on

char packetBuffer[255];          // buffer to hold incoming packet
char ReplyBuffer[] = "ACK";      // a string to send back

WiFiEspUDP Udp;

//int packetSize = Udp.parsePacket();

void setup() {

  pinMode(BRAUN_10, INPUT);
  pinMode(WEISS_BRAUN_20, INPUT);
  pinMode(GELB_30, INPUT);
  pinMode(WEISS_GELB_40, INPUT);
  pinMode(SCHWARZ_50, INPUT);
  pinMode(WEISS_SCHWARZ_60, INPUT);
  pinMode(BLAU_ROT_70, INPUT);
  pinMode(ROT_80, INPUT);
  pinMode(BLAU_WEISS_90, INPUT);
  pinMode(WEISS_100, INPUT);

  pinMode (RELAIS, OUTPUT);
  pinMode (VENTIL, OUTPUT);

  pinMode (TASTER, INPUT_PULLUP);

  // initialize serial for debugging
  Serial.begin(9600);
  // initialize serial for ESP module
  Serial1.begin(115200);
  // initialize ESP module
  WiFi.init(&Serial1);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  
  Serial.println("Connected to wifi");
  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  Udp.begin(localPort);
  
  Serial.print("Listening on port ");
  Serial.println(localPort);

  //Setup for Densitron LCD Display

  pinMode(CS,   OUTPUT);
  pinMode(WR,   OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  lcd_init();

  //delay(2000);
}

void loop () {

// if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Serial.println("Contents:");
    Serial.println(packetBuffer);

    evalSerialData(packetBuffer);
  }

  else if (digitalRead(TASTER) == LOW) {  // Wenn auf der Tasters auf LOW gezogen wird ...

    Fuellstand();
    /*
        digitalWrite(RELAIS, HIGH); // Setze Relaisausgang auf HIGH

        p=0; // Rücksetzen des Pegelstandes zur Neumessung
        P=0;

        delay(2000); // Zeit, die der Kondensator zum Aufladen braucht, erst danach brauchbare Werte

        for (int channel = 0; channel<=9; channel++){
          if (analogRead(channel) > 100) { //Schwellwert 100, genau bei 0,7V etwa 140

          int a;
          a=analogRead(channel);

          Serial.print("A");
          Serial.print(channel);
          Serial.print(":");
          Serial.println(a);

          p++;

          }
        else
        {
          break;
        }
      }
    */
    Serial.print("p=");
    Serial.println(p);

    //p=f;

    //p=Fuellstand();
    P = 10 * p;

    Serial.print("Pegelhöhe: ");
    Serial.print(P);
    Serial.println("%");

    // delay(1000);

    Temperatur();

    /*
        float X, R, T1;

        X = analogRead(A10);

        R = ((1023/X) - 1)*(1800);

        T1 = ((R - 1625)/15);

        T =(int) T1 - Offset;


      //  t = Temperatur;

        Serial.print("X=");
        Serial.println(X);
        Serial.print("R=");
        Serial.println(R);
        Serial.print("T1=");
        Serial.println(T1);
        Serial.print("Temperatur: ");
        Serial.print(T);
        Serial.println("°C");

    */

    String Ausgabe = "P" + String(P) + "%" + "," + "T" + String(T) + "G";

    //Serial3.println(Ausgabe);
    UDPsend(Ausgabe);

    /*
      Serial3.print("P");
      Serial3.print(P);
      Serial3.println("%");


      Serial3.print("T");
      Serial3.print(T);
      Serial3.println("G");
    */

    if (p == 10) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('1'));
      lcd_update_buf(1, char2seg('0'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }

    else if (p == 9) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('9'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 8) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('8'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 7) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('7'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 6) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('6'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 5) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('5'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 4) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('4'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 3) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('3'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 2) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('2'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }
    else if (p == 1) {
      lcd_clear_buf();
      lcd_update_buf(0, char2seg('P'));
      lcd_update_buf(1, char2seg('1'));
      lcd_update_buf(2, char2seg('0'));
      lcd_flush_buf();
    }

    delay (2000);

    int t1, t2;

    t1 = (int) T / 10;
    t2 = T - 10 * t1;

    lcd_clear_buf();
    lcd_update_buf(0, char2seg('T'));

    if (t1 == 2) {
      lcd_update_buf(1, char2seg('2'));
    }
    else if (t1 == 1) {
      lcd_update_buf(1, char2seg('1'));
    }
    else if (t1 == 0) {
      lcd_update_buf(1, char2seg(' '));
    }
    if (t2 == 9) {
      lcd_update_buf(2, char2seg('9'));
    }
    else if (t2 == 8) {
      lcd_update_buf(2, char2seg('8'));
    }
    else if (t2 == 7) {
      lcd_update_buf(2, char2seg('7'));
    }
    else if (t2 == 6) {
      lcd_update_buf(2, char2seg('6'));
    }
    else if (t2 == 5) {
      lcd_update_buf(2, char2seg('5'));
    }
    else if (t2 == 4) {
      lcd_update_buf(2, char2seg('4'));
    }
    else if (t2 == 3) {
      lcd_update_buf(2, char2seg('3'));
    }
    else if (t2 == 2) {
      lcd_update_buf(2, char2seg('2'));
    }
    else if (t2 == 1) {
      lcd_update_buf(2, char2seg('1'));
    }
    else if (t2 == 0) {
      lcd_update_buf(2, char2seg('0'));
    }
    lcd_flush_buf();

    delay(2000);

    digitalWrite(RELAIS, LOW);

  }
    else
  {

    Serial.print("Pegelhöhe: ");
    Serial.print(P);
    Serial.println("%");

    delay (1000);

    Serial.print("Temperatur: ");
    Serial.print(T);
    Serial.println("°C");

    if (P <= 10 && P > 0) {
      digitalWrite(VENTIL, HIGH);
      Serial.println("Notspeisung aktiviert!");
      //Serial3.println("668"); // N=6, O=6, T=8
      UDPsend("668");

      lcd_clear_buf();
      lcd_update_buf(0, char2seg('N'));
      lcd_update_buf(1, char2seg('O'));
      lcd_update_buf(2, char2seg('T'));
      lcd_flush_buf();

    }
    else {
      digitalWrite(VENTIL, LOW);
    }

    if ((millis() / 3000) % 2 == 1) {

      if (p == 10) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg('1'));
        lcd_update_buf(1, char2seg('0'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }

      else if (p == 9) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('9'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 8) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('8'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 7) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('7'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 6) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('6'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 5) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('5'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 4) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('4'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 3) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('3'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 2) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('2'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();
      }
      else if (p == 1) {
        lcd_clear_buf();
        lcd_update_buf(0, char2seg(' '));
        lcd_update_buf(1, char2seg('1'));
        lcd_update_buf(2, char2seg('0'));
        lcd_flush_buf();

      }
    }

    else {

      int t1, t2;

      t1 = (int) T / 10;
      t2 = T - 10 * t1;

      lcd_clear_buf();
      lcd_update_buf(0, char2seg('T'));

      if (t1 == 2) {
        lcd_update_buf(1, char2seg('2'));
      }
      else if (t1 == 1) {
        lcd_update_buf(1, char2seg('1'));
      }
      else if (t1 == 0) {
        lcd_update_buf(1, char2seg(' '));
      }
      if (t2 == 9) {
        lcd_update_buf(2, char2seg('9'));
      }
      else if (t2 == 8) {
        lcd_update_buf(2, char2seg('8'));
      }
      else if (t2 == 7) {
        lcd_update_buf(2, char2seg('7'));
      }
      else if (t2 == 6) {
        lcd_update_buf(2, char2seg('6'));
      }
      else if (t2 == 5) {
        lcd_update_buf(2, char2seg('5'));
      }
      else if (t2 == 4) {
        lcd_update_buf(2, char2seg('4'));
      }
      else if (t2 == 3) {
        lcd_update_buf(2, char2seg('3'));
      }
      else if (t2 == 2) {
        lcd_update_buf(2, char2seg('2'));
      }
      else if (t2 == 1) {
        lcd_update_buf(2, char2seg('1'));
      }
      else if (t2 == 0) {
        lcd_update_buf(2, char2seg('0'));
      }
      lcd_flush_buf();
    }
  }
}
//Ende void loop ab jetzt Funktionen

/*
void serialEvent()
{
  int len = Udp.read(packetBuffer, 255);
  if (len > 0) {
      packetBuffer[len] = 0;
  }
    evalSerialData();
}
*/
void evalSerialData(char data[])
{
  Serial.println(data); // zum Debuggen

  if (strcmp(data, "668") == 0) {

    Fuellstand();

    delay(2000);

    P = 10 * p;

    Serial.print("Pegelhöhe:");
    Serial.print(P);
    Serial.print("%");

    /*
      Serial3.print("P");
      Serial3.print(P);
      Serial3.println("%");
    */

    Temperatur();

    Serial.print("Temperatur:");
    Serial.print(T);
    Serial.println("°C");

    /*
      Serial3.print("T");
      Serial3.print(T);
      Serial3.println("G");
    */
    String Ausgabe = "P" + String(P) + "%" + "," + "T" + String(T) + "G";

    //Serial3.println(Ausgabe);
    UDPsend(Ausgabe);

    digitalWrite(RELAIS, LOW);
  }

  else if (strcmp(data, "836") == 0) {
    digitalWrite(VENTIL, HIGH);
    Serial.println("Ventil via UART eingeschaltet");
    //Serial3.println("836");
    UDPsend("836");
  }

  else if (strcmp(data, "837") == 0) {
    digitalWrite(VENTIL, LOW);
    Serial.println("Ventil via UART ausgeschaltet");
    //Serial3.println("837");
    UDPsend("837");
  }

  else {
    Serial.println("Falsches Kommando");
    //Serial3.println ("Error");
    UDPsend("Error");
  }
}


int Fuellstand () {

  digitalWrite(RELAIS, HIGH); // Setze Relaisausgang auf HIGH

  p = 0; // Rücksetzen des Pegelstandes zur Neumessung
  P = 0;

  delay(2000); // Zeit, die der Kondensator zum Aufladen braucht, erst danach brauchbare Werte

  for (int channel = 0; channel <= 9; channel++) {
    if (analogRead(channel) > 100) { //Schwellwert 100, genau bei 0,7V etwa 140

      int a;
      a = analogRead(channel);

      Serial.print("A");
      Serial.print(channel);
      Serial.print(":");
      Serial.println(a);

      p++;

    }
    else
    {
      break;
    }
  }
  return p;
}


int Temperatur() {

  float X, R, T1;

  X = analogRead(A10);

  R = ((1023 / X) - 1) * (1800);

  T1 = ((R - 1625) / 15);

  T = (int) T1 - Offset;

  //  t = Temperatur;

  Serial.print("X=");
  Serial.println(X);
  Serial.print("R=");
  Serial.println(R);
  Serial.print("T1=");
  Serial.println(T1);
  Serial.print("Temperatur: ");
  Serial.print(T);
  Serial.println("°C");

  return T;

}

void UDPsend (String text) {
  Udp.beginPacket(RecipientIP, RecipientPort);
  Udp.print(text);
  Udp.endPacket();
  delay(10);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
