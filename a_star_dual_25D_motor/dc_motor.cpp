// DCMotor class
// derived from Amanda Ghassaei's DCMotorCode project:
// https://github.com/amandaghassaei/DCMotorCode

#include "DC_motor.h"

DCMotor::DCMotor(byte dir_pin1, byte dir_pin2, byte pwm_pin, byte current_sense_pin, byte encA_pin, byte encB_pin, int counts_per_revolution)
{
  _dir_pin1 = dir_pin1;
  _dir_pin2 = dir_pin2;
  _pwm_pin = pwm_pin;
  _current_sense_pin = current_sense_pin;
  _encA_pin = encA_pin;
  _encB_pin = encB_pin;
  _enc_trig_flag = false;
  
  _position = 0;
  _desired_position = _position;
  
  _velocity = 0;
  _desired_velocity = _velocity;
  _delta_T = 0;
  _counts_per_revolution = counts_per_revolution;
  
  _current = 0;
  _current_sensitivity = 0.03491; //A / ADC Val  //140mV/A
  _desired_current = int(0.8/_current_sensitivity);
  
  _currentFilter = new LPFilter();
  _f_cs_smoothing = 2.5; //Hz
  
  _polarity = 0;
  
  _k_p = 0;
  _k_i = 0;
  _k_d = 0;
}

void DCMotor::init()
{
  pinMode(_dir_pin1, OUTPUT);
  pinMode(_dir_pin2, OUTPUT);
  pinMode(_pwm_pin, OUTPUT);
  pinMode(_current_sense_pin, INPUT);
  pinMode(_encA_pin, INPUT);
  pinMode(_encB_pin, INPUT);   
  
  _t_enc_triggered = micros();
  _last_t_enc_triggered = _t_enc_triggered;
  
  _currentFilter->init(_f_cs_smoothing, this->_measureCurrent());
}

void DCMotor::setCurrentLimit(float current_limit)
{
  _current_limit = current_limit;
}

void DCMotor::setPWMLimit(int max_pwm)
{
  _max_pwm = max_pwm;
}

void DCMotor::setPIDGains(int p, int i, int d) 
{
  _k_p = p;
  _k_i = i;
  _k_d = d;
}

float DCMotor::_measureCurrent() 
{
  return analogRead(_current_sense_pin)*_current_sensitivity; 
}

long DCMotor::getPosition()
{
  return _position;
}

void DCMotor::update()
{
  calculateVelocity();
  _last_error = _error;
  _error = _desired_velocity-_velocity;
  _delta_error = _error-_last_error;
  _sum_error += _error;
  _sum_error = constrain(_sum_error, -100, 100); // TODO WHAT'S REASONABLE WINDUP LIMIT?
  int pwr = int(_k_p * _error + _k_i * _sum_error + _k_d * _delta_error);
  drive(pwr);
}

void DCMotor::setDesiredVelocity(float desired_velocity) 
{
  _desired_velocity = desired_velocity;
}

void DCMotor::setPolarity(int8_t polarity)
{
  _polarity = polarity;
}

void DCMotor::drive(int power)
{
  if (_sgn(power) > 0) {
    if (!_polarity) {
      digitalWrite(_dir_pin1, HIGH);
      digitalWrite(_dir_pin2, LOW);
    } else {
      digitalWrite(_dir_pin1, LOW);
      digitalWrite(_dir_pin2, HIGH);
    }
  } else if (_sgn(power) < 0) {
    if (!_polarity) {
      digitalWrite(_dir_pin1, LOW);
      digitalWrite(_dir_pin2, HIGH);
    } else {
      digitalWrite(_dir_pin1, HIGH);
      digitalWrite(_dir_pin2, LOW);
    }
  } else {
    // if power = 0, coast
    digitalWrite(_dir_pin1, LOW);
    digitalWrite(_dir_pin2, LOW);
  }
  uint8_t pwr = constrain(abs(power),0,_max_pwm);
  analogWrite(_pwm_pin, pwr);
}

float DCMotor::calculateVelocity()
{
  // TO DO: add timeout for velocity = 0?
  if (_enc_trig_flag) {
    // encoder triggered
    _delta_T = _t_enc_triggered-_last_t_enc_triggered;
    if (_delta_T > 0) _velocity = 1000000/_delta_T; // counts/sec
    _enc_trig_flag = false;
    _last_trig = millis();
  } else {
    // no encoder tick, are we stopped?
    if (millis()-_last_trig > 100) { // 100 --> mininum measurable speed = 10 counts/sec
      _velocity = 0;
    }
  }
  
  _velocity = constrain(_velocity,0,3200); // determined experimentally by spinnning motor at full power
  // max no load velocity = 3000.00 @ 6V, 255 pwm (about .145A current drawn from power supply)
  
  return _velocity;
}

void DCMotor::interruptRoutineA()
{
  if (digitalRead(_encA_pin)) {
    if (!digitalRead(_encB_pin)) { _position++; } 
    else { _position--; }
  } else {
    if (!digitalRead(_encB_pin)) { _position--; } 
    else { _position++; }
  }
  
  _last_t_enc_triggered = _t_enc_triggered;
  _t_enc_triggered = micros();
  _enc_trig_flag = true;
}

void DCMotor::interruptRoutineB()
{
  // todo: figure out gray code for properly incrementing position after being triggered by encB
  _last_t_enc_triggered = _t_enc_triggered;
  _t_enc_triggered = micros();
  _enc_trig_flag = true;
}

int8_t DCMotor::_sgn(int val) {
  if (val < 0) return -1;
  if (val==0) return 0;
  return 1;
}
