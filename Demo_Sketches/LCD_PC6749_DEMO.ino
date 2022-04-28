/* 
 Test für Segment Display
*/

int CS = 2;
int DATA = 3;
int WR = 4;

#define CS_low()   digitalWrite(CS, LOW); delayMicroseconds(1)
#define CS_high()  delayMicroseconds(1); digitalWrite(CS, HIGH)
#define DATA(x)    if ((x)==0) digitalWrite(DATA, LOW); else digitalWrite(DATA, HIGH)
#define WR_pulse() digitalWrite(WR, LOW); delayMicroseconds(3); digitalWrite(WR, HIGH); delayMicroseconds(3)
//#define LED_on()   PORTB |= 0x20
//#define LED_off()  PORTB &= ~0x20

/* Definitions for the Densitron PC-6749 LCD glass (Pollin 120818)
 * this is a 3-digit 16-segment LC display controlled by a HT1621
 *
 *   glass segment mapping
 * from the display datasheet  |  and how we use it
 *
 * ---------------------------
 *      | COM0 COM1 COM2 COM3
 * -----+---------------------
 * SEG0 | 1G   1K   1F   1E    \ lcd_buf[0] bit 0  1  2  3
 * SEG1 | 1H   1L   1N   1D    /                4  5  6  7
 *
 * SEG2 | 1I   1M   1O   1P    \ lcd_buf[1] bit 0  1  2  3
 * SEG3 | 1A   1J   1B   1C    /                4  5  6  7
 *
 * SEG4 |       X    Y    Z    \ lcd_buf[2] bit 0  1  2  3
 * SEG5 | 2G   2K   2F   2E    /                4  5  6  7
 *
 * SEG6 | 2H   2L   2N   2D    \ lcd_buf[3] bit 0  1  2  3
 * SEG7 | 2I   2M   2O   2P    /                4  5  6  7
 *
 * SEG8 | 2A   2J   2B   2C    \ lcd_buf[4] bit 0  1  2  3
 * SEG9 | 3G   3K   3F   3E    /                4  5  6  7
 *
 * SEG10| 3H   3L   3N   3D    \ lcd_buf[5] bit 0  1  2  3
 * SEG11| 3I   3M   3O   3P    /                4  5  6  7
 *
 * SEG12| 3A   3K   3B   3C    } lcd_buf[6] bit 0  1  2  3
 * ---------------------------
 */

/*
 * segment mapping table [digit][segment]
 * entry is lcd_buf address in high nibble
 * and bit number in low nibble
 */
const uint8_t segmap[3][16]=
{
    { 0x14, 0x16, 0x17, 0x07, 0x03, 0x02, 0x00, 0x04,
      0x10, 0x15, 0x01, 0x05, 0x11, 0x06, 0x12, 0x13 },
    { 0x40, 0x42, 0x43, 0x33, 0x27, 0x26, 0x24, 0x30,
      0x34, 0x41, 0x25, 0x31, 0x35, 0x32, 0x36, 0x37 },
    { 0x60, 0x62, 0x63, 0x53, 0x47, 0x46, 0x44, 0x50,
      0x54, 0x61, 0x45, 0x51, 0x55, 0x52, 0x56, 0x57 }
};

/*
 * font for 16-segment display
 * it is sparse, hence organized as list
 */

const struct {
    char c;     /* the character */
    uint16_t s; /* 16 bits for segments A..P */
}  seg16font[]=
{         /* ABCDEFGHIJKLMNOP */
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
 * search character in font, return segments
 * nonexisting character yields empty pattern
 */
uint16_t char2seg(char c)
{
    for (uint8_t j=0; seg16font[j].c; j++) {
        if (seg16font[j].c == c) {
            return seg16font[j].s;
        }
    }
    return 0;
}


/*
 * Send a command to the LCD controller
 * ! we send the command code MSB first !
 *  so it matches the datasheet notation
 */
void lcd_cmd(uint8_t cmd) {
    CS_low();
    DATA(1); WR_pulse();
    DATA(0); WR_pulse();
    DATA(0); WR_pulse();
    uint8_t mask= 0x80;
    while (mask) {
        DATA(cmd&mask);
        WR_pulse();
        mask>>=1;
    }
    DATA(1); WR_pulse();
    CS_high();
}

/*
 * initialize and enable the LCD controller
 */
void lcd_init(void) {
    lcd_cmd(0b00000001); // system enable
    lcd_cmd(0b00101011); // 1/3 Bias, 4 commons
    lcd_cmd(0b00000011); // turn on LCD
}

/*
 * the LCD buffer
 */
#define LCD_BUF_SIZE 7
uint8_t lcd_buf[LCD_BUF_SIZE];

/*
 * clear LCD buffer
 */
void lcd_clear_buf(void)
{
    for (uint8_t i=0; i<LCD_BUF_SIZE; i++) {
        lcd_buf[i]= 0;
    }
}

/*
 * set segment bits from 'seg' for digit at position 'pos'
 */
void lcd_update_buf(uint8_t pos, uint16_t seg)
{
    uint8_t segno= 0;
    uint16_t mask= 0x8000;
    for ( ; segno<16; segno++, mask>>=1) {
        if (seg & mask) {
            uint8_t code= segmap[pos][segno];
            lcd_buf[code>>4] |= 1<<(code&7);
        }
    }
}

/*
 * flush LCD buffer to the LCD controller
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
    for (uint8_t i=0; i<LCD_BUF_SIZE; i++) {
        uint8_t mask= 0x01;
        while (mask) {
            DATA(lcd_buf[i] & mask);
            WR_pulse();
            mask<<=1;
        }
    }
    DATA(1);
    CS_high();
}

void setup() 
{
 Serial.begin(9600);

 pinMode(CS,   OUTPUT);
 pinMode(WR,   OUTPUT);
 pinMode(DATA, OUTPUT);
 pinMode(LED_BUILTIN, OUTPUT);

  lcd_init();

}
 
int i=0;
 
void loop() 
{

 
      /* cycle through the font, displaying 3 characters at a time */
      for (uint8_t i=0; seg16font[i+1].c; i++) 
      {
          //LED_on();
          lcd_clear_buf();
          lcd_update_buf(0, seg16font[i].s);
          lcd_update_buf(1, seg16font[i+1].s);
          lcd_update_buf(2, seg16font[i+2].s);
          lcd_flush_buf();
          //LED_off();
 
          digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
 
        delay(400);
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
          
          Serial.println(i);      
          delay(200);
 
      }


 
 
 
 
}
