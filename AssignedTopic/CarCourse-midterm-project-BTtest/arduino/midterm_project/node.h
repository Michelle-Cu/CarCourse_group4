/***************************************************************************/
// File			  [node.h]
// Author		  [Erik Kuo, Joshua Lin]
// Synopsis		[Code for managing car movement when encounter a node]
// Functions  [/* add on your own! */]
// Modify		  [2020/03/027 Erik Kuo]
/***************************************************************************/

/*===========================import variable===========================*/
int extern _Tp;
/*===========================import variable===========================*/

// TODO: add some function to control your car when encounter a node
// here are something you can try: left_turn, right_turn... etc.

void stop_motors() {
    digitalWrite(MotorL_I3, LOW);
    digitalWrite(MotorL_I4, LOW);
    digitalWrite(MotorR_I1, LOW);
    digitalWrite(MotorR_I2, LOW);
    analogWrite(MotorL_PWML, 0);
    analogWrite(MotorR_PWMR, 0);
}

void left_turn(int lSpeed, int rSpeed) {
    // left motor goes backward, right motor goes forward
    digitalWrite(MotorL_I3, LOW);
    digitalWrite(MotorL_I4, HIGH);
    digitalWrite(MotorR_I1, HIGH);
    digitalWrite(MotorR_I2, LOW);
    analogWrite(MotorL_PWML, lSpeed);
    analogWrite(MotorR_PWMR, rSpeed);
}

void right_turn(int lSpeed, int rSpeed) {
    // left motor goes forward, right motor goes backward
    digitalWrite(MotorL_I3, HIGH);
    digitalWrite(MotorL_I4, LOW);
    digitalWrite(MotorR_I1, LOW);
    digitalWrite(MotorR_I2, HIGH);
    analogWrite(MotorL_PWML, lSpeed);
    analogWrite(MotorR_PWMR, rSpeed);
}

void u_turn() {
    // same as left_turn but longer — 180 degrees
    left_turn(150, 150);
    delay(700);  // tune this value on the actual track
    stop_motors();
}
