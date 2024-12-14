#include "C610.h"
#include <CAN.h>  // STM32 CAN library

namespace C610Helper {
  int sign(float val) { return (val > 0) - (val < 0); }
}

C610::C610() {
  _initialized_mechanical_angle = false;
  _rotations = 0;
  _last_pos_measurement = 0;
  _counts = 0;
  _rpm = 0;
  _current = 0;
}

C610Feedback C610::InterpretMessage(const CAN_message_t &msg) {
  // See C610 manual for protocol details
  C610Feedback feedback = {
      .counts = uint16_t((msg.buf[0] << 8) | msg.buf[1]),
      .rpm = int16_t((msg.buf[2] << 8) | msg.buf[3]),
      .current = int16_t((msg.buf[4] << 8) | msg.buf[5])};
  return feedback;
}

float C610::Position() {
  return _counts / kCountsPerRad;
}

float C610::Velocity() {
  return _rpm / kRPMPerRadS;
}

float C610::ElectricalPower() {
  return Current() * Current() * kResistance + kVoltageConstant * Velocity() * Current();
}

float C610::Torque() {
  if (_rpm * _current > 0) {
    return -0.0673 * C610Helper::sign(Velocity()) - 0.00277 * Velocity() + 0.000308 * _current;
  } else {
    return -0.0136 * C610Helper::sign(Velocity()) - 0.00494 * Velocity() + 0.000179 * _current;
  }
}

float C610::MechanicalPower() {
  return Torque() * Velocity();
}

float C610::Current() {
  return _current / kMilliAmpPerAmp;
}

void C610::UpdateState(C610Feedback feedback) {
  // Initial setup
  if (!_initialized_mechanical_angle) {
    _initialized_mechanical_angle = true;
    _last_pos_measurement = feedback.counts;
  }

  // Position calculation
  int32_t delta = feedback.counts - _last_pos_measurement;
  if (delta > kCountsPerRev / 2) {
    _rotations -= 1;
  } else if (delta < -kCountsPerRev / 2) {
    _rotations += 1;
  }
  _counts = _rotations * kCountsPerRev + feedback.counts;
  _last_pos_measurement = feedback.counts;

  // Update velocity and torque
  _rpm = feedback.rpm;
  _current = feedback.current;
}
