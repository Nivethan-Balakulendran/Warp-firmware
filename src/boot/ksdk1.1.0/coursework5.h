void readaccel();
void clearscreen();
void configureScale(char scale);
void fillscreen (uint8_t red, uint8_t green, uint8_t blue);
void text(uint8_t startrow, uint8_t startcol, char character, uint8_t red, uint8_t green, uint8_t blue);
void rectdraw(uint8_t startcol, uint8_t startrow, uint8_t endcol, uint8_t endrow);
void textcol(uint8_t red, uint8_t green, uint8_t blue);
void word(uint8_t startcol, uint8_t startrow, char gesture, uint8_t red, uint8_t green, uint8_t blue);
