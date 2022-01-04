#include <Arduino.h>
#include <ESP8266WiFi.h>

// changeme!
#define WIFI_SSID "test"
#define WIFI_PASS "test"

// MAX7219 connection
#define DATA 13
#define CLK 14
#define LOAD 15
#define COUNT 3
#define LED_BRIGHT 2

#include "LedControl.h"
LedControl lc=LedControl(DATA,CLK,LOAD,COUNT);
#include <ModbusTCP.h>
ModbusTCP mb;

// define modbus register + panel address
#define X_LED 0
#define X_SIGN 1
#define X_INT 2
#define X_DEC 3

#define Y_LED 1
#define Y_SIGN 4
#define Y_INT 5
#define Y_DEC 6

#define Z_LED 2
#define Z_SIGN 7
#define Z_INT 8
#define Z_DEC 9

// ---- 
uint8_t led_buffer[3][8] = {
  { 0xfe, 0xfe, 0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0x80 },
  { 0xfe, 0xfe, 0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0x80 },
  { 0xfe, 0xfe, 0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0x80 },
};
  
const uint8_t SEG_P = 0x01;
const uint8_t SEG_A = 0x02;
const uint8_t SEG_B = 0x04;
const uint8_t SEG_C = 0x08;
const uint8_t SEG_D = 0x10;
const uint8_t SEG_E = 0x20;
const uint8_t SEG_F = 0x40;
const uint8_t SEG_G = 0x80;


const uint8_t SEG_numbers[] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,          // 0
  SEG_B | SEG_C,                                          // 1
  SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                  // 2
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                  // 3
  SEG_B | SEG_C | SEG_F | SEG_G,                          // 4
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  // 5
  SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          // 6
  SEG_A | SEG_B | SEG_C,                                  // 7
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,  // 8
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,          // 9
};


int xbuf[3] = { 1, 0, 0 };
int ybuf[3] = { 1, 0, 0 };
int zbuf[3] = { 1, 0, 0 };

void write_to_led(int panel) {
  if (panel <= lc.getDeviceCount()) {
    for(int row=0;row<8;row++) {
      for(int col=0;col<8;col++) {
         bool led_segment = led_buffer[panel][row] & (1 << col);
         lc.setLed(panel,row,col,led_segment);
      }
    }
  }
}

void led_clear(int panel) {
  if (panel <= lc.getDeviceCount()) {
    for(int row=0;row<8;row++) {
      led_buffer[panel][row] = 0x00;
    }
    write_to_led(panel);
  }
}

void led_write_number(int panel, char number_string[]) {
  uint8_t led_pos = 8;
  for (int x = 0; x<9; x++) {
    if (number_string[x] == '.') {
      led_buffer[panel][led_pos] = led_buffer[panel][led_pos] | SEG_P;
    } else {
      led_pos = led_pos - 1;
      if (number_string[x] == '-') {
        led_buffer[panel][led_pos] = SEG_G;
      } else if (number_string[x] == ' ') {
       led_buffer[panel][led_pos] = 0x00;
      } else {
        uint8_t number = int(number_string[x]) - 48;
        led_buffer[panel][led_pos] = SEG_numbers[number];
      }
    }
  }
  write_to_led(panel);
}

void write_position(int panel, int positon[]) {
  char bufs[9];
  if (positon[0] > 0) {
    sprintf(bufs, "-%04d.%03d", positon[1], positon[2]);
  } else {
    sprintf(bufs, " %04d.%03d", positon[1], positon[2]);
  }
  led_write_number(panel, bufs); 
}

void setup()
{
  Serial.begin(115200);
  Serial.println("init display");

  int devices=lc.getDeviceCount();
  for(int address=0;address<devices;address++) {
    lc.shutdown(address,false);
    lc.setIntensity(address,LED_BRIGHT);
    lc.clearDisplay(address);
  }
  write_to_led(X_LED);
  write_to_led(Y_LED);
  write_to_led(Z_LED);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  mb.server();
  for (int reg=0; reg<10; reg++) {
    mb.addHreg(reg, 0x0);
  }
  
  write_position(X_LED, xbuf);
  write_position(Y_LED, ybuf);
  write_position(Z_LED, zbuf);

  Serial.println("starting main");
}

void loop() {
  mb.task();

  if (mb.Hreg(X_DEC) != xbuf[2]) {
    xbuf[0] = mb.Hreg(X_SIGN);
    xbuf[1] = mb.Hreg(X_INT);
    xbuf[2] = mb.Hreg(X_DEC);
    write_position(X_LED, xbuf);
  }
  
  if (mb.Hreg(Y_DEC) != ybuf[2]) {
    ybuf[0] = mb.Hreg(Y_SIGN);
    ybuf[1] = mb.Hreg(Y_INT);
    ybuf[2] = mb.Hreg(Y_DEC);
    write_position(Y_LED, ybuf);
  }

  if (mb.Hreg(Z_DEC) != zbuf[2]) {
    zbuf[0] = mb.Hreg(Z_SIGN);
    zbuf[1] = mb.Hreg(Z_INT);
    zbuf[2] = mb.Hreg(Z_DEC);
    write_position(Z_LED, zbuf);
  }

  delay(25);
}
