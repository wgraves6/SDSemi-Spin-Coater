const int IN1 = 5;
const int IN2 = 4;
const int ENA = 6;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
}


void loop() {
  Motor1_Brake();
  
  /*=================================================

  This is where you spin profile
  It uses PWM. Goes from 0-off to 255 always on. RPM is not linear.
  255PWM = 5000rpm
  Motor forwards take a PWM value input

  ===================================================*/
  delay(1000);
  Motor1_Forward(50); //initial speed roughly 500 rpm
  delay(30000); // 30 secs
  Motor1_Forward(255); //top speed full pwm, 5000 rpm
  delay(60000); // 60 secs


  Motor1_Brake();
  while (true); // stops running
}


/*
XY160D Driver Module
*/
void Motor1_Forward(int Speed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, Speed);
}


void Motor1_Backward(int Speed) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, Speed);
}
void Motor1_Brake() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}
