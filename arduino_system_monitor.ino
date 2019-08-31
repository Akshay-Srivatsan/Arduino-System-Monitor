#include <LiquidCrystal.h>
#include <LCDKeypad.h>
#include <TimeLib.h>

LCDKeypad lcd;

enum Mode {
  BOOT_MODE,
  TIME_MODE,
  SYSTEM_STATS_MODE,
  MUSIC_MODE,
};

Mode current_mode = BOOT_MODE;

bool has_valid_time = false;
bool autocycle = false;
int cpu_percentage = 0;
int mem_percentage = 0;

bool is_playing_music = false;
bool is_paused = false;
String track_name = "";
String artist_name = "";
String player_name = "";
int name_scroll_position = 0;
unsigned long last_scroll_time = 0;
#define SCROLL_TIME 150

unsigned long last_sync = 0;
#define SYNC_TIME 1000
unsigned long last_redraw = 0;
#define REDRAW_TIME 250
unsigned long autocycle_time = 0;
#define AUTOCYCLE_TIME 3000

unsigned long last_connect_time = 0;
#define MAX_DURATION_BETWEEN_RESPONSES 5000

char display_line_1[16];
char display_line_2[16];

char old_display_line_1[16];
char old_display_line_2[16];

byte null_character[8] = {
  B11111,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B11111,
};

byte plug_character[8] = {
  B00000,
  B01010,
  B01010,
  B11111,
  B11111,
  B01110,
  B00100,
  B00100,
};

byte cycle_character[8] = {
  B01100,
  B10010,
  B10010,
  B10111,
  B10010,
  B10000,
  B10010,
  B01100,
};

byte bar_0_character[8] = {
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
};

byte bar_1_character[8] = {
  B11111,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11111,
};

byte bar_2_character[8] = {
  B11111,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11111,
};

byte bar_3_character[8] = {
  B11111,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11111,
};

byte bar_4_character[8] = {
  B11111,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11111,
};

byte bar_5_character[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
};

byte stopped_character[8] = {
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000,
  B00000,
};

byte playing_character[8] = {
  B10000,
  B11000,
  B11100,
  B11110,
  B11100,
  B11000,
  B10000,
  B00000,
};

byte paused_character[8] = {
  B00000,
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
  B00000,
  B00000,
};

void setup() {
  Serial.begin(9600);
  Serial.println("Hello, World!");
  lcd.begin(16, 2);
  memset(display_line_1, '#', 16);
  memset(display_line_2, '#', 16);
}

void loop() {
  handle_input();
  handle_mode();
  handle_display();
}

void handle_input() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    if (line.startsWith("T")) {
      time_input(line);
    } else if (line.startsWith("C")) {
      cpu_input(line);
    } else if (line.startsWith("M")) {
      mem_input(line);
    } else if (line.startsWith("N") || line.startsWith("A") || line.startsWith("P")) {
      music_input(line);
    }
    last_connect_time = millis();
  }
}

void time_input(String line) {
  String time_str = line.substring(1);
  time_t unixtime_shifted = time_str.toInt();
  setTime(unixtime_shifted);
  has_valid_time = true;
}

void cpu_input(String line) {
  String cpu_str = line.substring(1);
  cpu_percentage = cpu_str.toInt();
}

void mem_input(String line) {
  String mem_str = line.substring(1);
  mem_percentage = mem_str.toInt();
}

void music_input(String line) {
  String value = line.substring(1);
  value.trim();
  if (line[0] == 'N') {
    if (value == "Spotify Premium") {
      is_paused = true;
    } else if (value != "") {
      is_paused = false;
      if (value != track_name) {
        name_scroll_position = 0;
        last_scroll_time = millis();
      }
      track_name = value;
    }
  } else if (line[0] == 'A') {
    if (value != "") {
      if (value != artist_name) {
        name_scroll_position = 0;
        last_scroll_time = millis();
      }
      artist_name = value;
    }
  } else if (line[0] == 'P') {
    if (value == "") {
      is_playing_music = false;
    } else {
      is_playing_music = true;
      player_name = value;
    }
  }
}

void handle_mode() {
  if (current_mode == BOOT_MODE) {
    boot_mode();
  } else if (current_mode == TIME_MODE) {
    time_mode();
  } else if (current_mode == SYSTEM_STATS_MODE) {
    system_stats_mode();
  } else if (current_mode == MUSIC_MODE) {
    music_mode();
  }
}

void boot_mode() {
  memcpy(display_line_1, "  Disconnected  ", 16);
  memset(display_line_2, ' ', 16);

  if (has_valid_time) set_mode(TIME_MODE);
}

void time_mode() {
  char timestr[17];
  memset(timestr, ' ', 17);
  snprintf(timestr, 17, "    %02d:%02d:%02d    ", hour(), minute(), second());
  char datestr[17];
  memset(datestr, ' ', 17);
  snprintf(datestr, 17, "   %04d-%02d-%02d   ", year(), month(), day());

  memcpy(display_line_1, timestr, 16);
  memcpy(display_line_2, datestr, 16);
  if (lcd.button() == KEYPAD_LEFT) set_mode(MUSIC_MODE);
  if (lcd.button() == KEYPAD_RIGHT) set_mode(SYSTEM_STATS_MODE);
}

void system_stats_mode() {
  static int submode = 0;
  
  char topline[17];
  char bottomline[16];
  memset(topline, ' ', 17);
  memset(bottomline, ' ', 17);
  
  if (submode == 0) {
    snprintf(topline, 17, "CPU: %2d%% %c %02d:%02d", cpu_percentage, autocycle ? 7 : ' ', hour(), minute());
    memcpy(display_line_1, topline, 16);
    float increment = 100.0/16.0;
    for (int i = 0; i < 16; i++) {
      float start = i*increment;
      if (cpu_percentage < start) {
        display_line_2[i] = 0;
      } else if (cpu_percentage < start + 0.2 * increment) {
        display_line_2[i] = 1;
      } else if (cpu_percentage < start + 0.4 * increment) {
        display_line_2[i] = 2;
      } else if (cpu_percentage < start + 0.6 * increment) {
        display_line_2[i] = 3;
      } else if (cpu_percentage < start + 0.8 * increment) {
        display_line_2[i] = 4;
      } else {
        display_line_2[i] = 5;
      }
    }
    if (lcd.button() == KEYPAD_DOWN) {submode = 1; set_autocycle(false);}
    if (lcd.button() == KEYPAD_SELECT) set_autocycle(!autocycle);
  } else if (submode == 1) {
    snprintf(topline, 17, "RAM: %2d%% %c %02d:%02d", mem_percentage, autocycle ? 7 : ' ', hour(), minute());
    memcpy(display_line_1, topline, 16);
    float increment = 100.0/16.0;
    for (int i = 0; i < 16; i++) {
      float start = i*increment;
      if (mem_percentage < start) {
        display_line_2[i] = 0;
      } else if (mem_percentage < start + 0.2 * increment) {
        display_line_2[i] = 1;
      } else if (mem_percentage < start + 0.4 * increment) {
        display_line_2[i] = 2;
      } else if (mem_percentage < start + 0.6 * increment) {
        display_line_2[i] = 3;
      } else if (mem_percentage < start + 0.8 * increment) {
        display_line_2[i] = 4;
      } else {
        display_line_2[i] = 5;
      }
    }
    if (lcd.button() == KEYPAD_UP) {submode = 0; set_autocycle(false);}
    if (lcd.button() == KEYPAD_SELECT) set_autocycle(!autocycle);
  }
  if (autocycle && millis() - autocycle_time > AUTOCYCLE_TIME) {
    submode = (submode + 1) % 2;
    autocycle_time = millis();
  }
  if (lcd.button() == KEYPAD_LEFT) set_mode(TIME_MODE);
  if (lcd.button() == KEYPAD_RIGHT) set_mode(MUSIC_MODE);
  if (!is_connected()) set_mode(TIME_MODE);
}

void music_mode() {
  static bool artist_mode = false;
  char topline[17];
  char bottomline[17];
  memset(topline, ' ', 17);
  memset(bottomline, ' ', 17);
  
  if (!is_playing_music) {
    snprintf(topline, 17, "%c          %02d:%02d", 0, hour(), minute());
    snprintf(bottomline, 17, "    No Track    ");
  } else {
    String text = artist_mode ? artist_name : track_name;
    snprintf(topline, 17, "%c %c        %02d:%02d", is_paused ? 2 : 1, autocycle ? 7 : ' ', hour(), minute());
    if (text.length() <= 16) {
      char buf[17];
      text.toCharArray(buf, 17);
      snprintf(bottomline, 17, "%-16s", buf);
    } else {
      text += "   ";
      if (name_scroll_position > text.length()) {
        name_scroll_position = 0;
      }
      for (int i = 0; i < 16; i++) {
        int offset_index = i + name_scroll_position;
        int wrapped_index = offset_index % text.length();
        bottomline[i] = text[wrapped_index];
      }
    }
  }

  if (millis() - last_scroll_time > SCROLL_TIME) {
    name_scroll_position += 1;
    last_scroll_time = millis();
  }

  memcpy(display_line_1, topline, 16);
  memcpy(display_line_2, bottomline, 16);
  
  if (lcd.button() == KEYPAD_LEFT) set_mode(SYSTEM_STATS_MODE);
  if (lcd.button() == KEYPAD_RIGHT) set_mode(TIME_MODE);
  if (lcd.button() == KEYPAD_SELECT) set_autocycle(!autocycle);
  if (lcd.button() == KEYPAD_UP) {artist_mode = !artist_mode; set_autocycle(false);}
  if (lcd.button() == KEYPAD_DOWN) {artist_mode = !artist_mode; set_autocycle(false);}
  if (!is_connected()) set_mode(TIME_MODE);
  
  if (autocycle && millis() - autocycle_time > AUTOCYCLE_TIME) {
    artist_mode = !artist_mode;
    autocycle_time = millis();
  }
}

void set_mode(Mode mode) {
  while (lcd.button() != KEYPAD_NONE);
  current_mode = mode;
  autocycle = false;
  if (mode == TIME_MODE) time_charset();
  else if (mode == SYSTEM_STATS_MODE) stats_charset();
  else if (mode == MUSIC_MODE) music_charset();
}

void set_autocycle(bool on) {
  while (lcd.button() != KEYPAD_NONE);
  autocycle = on;
  autocycle_time = millis();
}


void time_charset() {
  lcd.createChar(0, null_character);
  lcd.createChar(1, null_character);
  lcd.createChar(2, null_character);
  lcd.createChar(3, null_character);
  lcd.createChar(4, null_character);
  lcd.createChar(5, null_character);
  lcd.createChar(6, null_character);
  lcd.createChar(7, null_character);
}

void stats_charset() {
  lcd.createChar(0, bar_0_character);
  lcd.createChar(1, bar_1_character);
  lcd.createChar(2, bar_2_character);
  lcd.createChar(3, bar_3_character);
  lcd.createChar(4, bar_4_character);
  lcd.createChar(5, bar_5_character);
  lcd.createChar(6, null_character);
  lcd.createChar(7, cycle_character);
}

void music_charset() {
  lcd.createChar(0, stopped_character);
  lcd.createChar(1, playing_character);
  lcd.createChar(2, paused_character);
  lcd.createChar(3, null_character);
  lcd.createChar(4, null_character);
  lcd.createChar(5, null_character);
  lcd.createChar(6, null_character);
  lcd.createChar(7, cycle_character);
}

bool is_connected() {
  return (millis() - last_connect_time) <= MAX_DURATION_BETWEEN_RESPONSES;
}

void handle_display() {
  bool has_changed = false;
  has_changed |= memcmp(display_line_1, old_display_line_1, 16);
  has_changed |= memcmp(display_line_2, old_display_line_2, 16);
  if (!has_changed) return;
  lcd.setCursor(0, 0);
  for (int i = 0; i < 16; i++) {
    lcd.write(display_line_1[i]);
  }
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.write(display_line_2[i]);
  }
  memcpy(old_display_line_1, display_line_1, 16);
  memcpy(old_display_line_2, display_line_2, 16);
}
