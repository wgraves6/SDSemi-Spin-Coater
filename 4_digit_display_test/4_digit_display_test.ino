#include <TM1637Display.h>
#define CLK 9
#define DIO 10

void setup() {
  TM1637Display display = TM1637Display(CLK, DIO);
  display.showNumberDec(-12); 
}

void loop() {
  // put your main code here, to run repeatedly:

}
