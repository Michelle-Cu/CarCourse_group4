/***************************************************************************/
// File			  [track.h]
// Author		  [Erik Kuo]
// Synopsis		[Code used for tracking]
// Functions  [MotorWriting, MotorInverter, tracking]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 1 & 6*/
/*feel free to add your own function for convenience*/

/*===========================import variable===========================*/
int extern _Tp;
/*===========================import variable===========================*/

// PID parameters (ported from April4th_remote.ino)
double _Kp = 50.0;
double _Kd = 1;
double _Ki = 0.0;
double _w0 = 0.0;  // weight for middle sensor
double _w1 = 1.0;  // weight for inner sensors
double _w2 = 3.0;  // weight for outer sensors
double lastError = 0;
double sumError = 0;

// Write the voltage to motor.
void MotorWriting(double vL, double vR) {
    // TODO: use TB6612 to control motor voltage & direction
    // right motor
    if (vR >= 0) {
        digitalWrite(MotorR_I1, HIGH);
        digitalWrite(MotorR_I2, LOW);
    } else {
        digitalWrite(MotorR_I1, LOW);
        digitalWrite(MotorR_I2, HIGH);
        vR = -vR;
    }
    // left motor
    if (vL >= 0) {
        digitalWrite(MotorL_I3, HIGH);
        digitalWrite(MotorL_I4, LOW);
    } else {
        digitalWrite(MotorL_I3, LOW);
        digitalWrite(MotorL_I4, HIGH);
        vL = -vL;
    }
    analogWrite(MotorR_PWMR, constrain(vR, 0, 255));
    analogWrite(MotorL_PWML, constrain(vL, 0, 255));
}  // MotorWriting

// Handle negative motor_PWMR value.
void MotorInverter(int motor, bool& dir) {
    // Hint: the value of motor_PWMR must between 0~255, cannot write negative value.
    if (motor < 0) {
        dir = !dir;
        motor = -motor;
    }
}  // MotorInverter

// P/PID control Tracking
void tracking(int l2, int l1, int m0, int r1, int r2) {
    // TODO: find your own parameters!
    // double _w0;  //
    // double _w1;  //
    // double _w2;  //
    // double _Kp;  // p term parameter
    // double _Kd;  // d term parameter (optional)
    // double _Ki;  // i term parameter (optional) (Hint: 不要調太大)
    // double error = l2 * _w2 + l1 * _w1 + m0 * _w0 + r1 * (-_w1) + r2 * (-_w2);
    // double vR, vL;  // 馬達左右轉速原始值(從PID control 計算出來)。Between -255 to 255.
    double adj_R = 0.85, adj_L = 1.15;  // 馬達轉速修正係數。MotorWriting(_Tp,_Tp)如果歪掉就要用參數修正。

    // // TODO: complete your P/PID tracking code

    // // end TODO
    // MotorWriting(adj_L * vL, adj_R * vR);

    double numerator = (l2 * -_w2) + (l1 * -_w1) + (m0 * _w0) + (r1 * _w1) + (r2 * _w2);
    double denominator = l2 + l1 + m0 + r1 + r2;
    if (denominator == 0) denominator = 0.0001;

    double error = numerator / denominator;
    double dError = error - lastError;
    sumError += error;
    sumError = constrain(sumError, -1000, 1000);

    double powerCorrection = (_Kp * error) + (_Kd * dError) + (_Ki * sumError);

    double vR = _Tp - powerCorrection;
    double vL = _Tp + powerCorrection;

    MotorWriting(adj_L * vL, adj_R * vR);
    lastError = error;
}  // tracking
