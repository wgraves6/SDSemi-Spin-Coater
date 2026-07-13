/*
  MPU6050.cpp - Driver for GY-521 (MPU-6050) 3-Axis Accelerometer + 3-Axis Gyroscope
  See MPU6050.h for wiring notes and usage.
*/

#include "MPU6050.h"

MPU6050::MPU6050(uint8_t address, TwoWire &wirePort) {
  _address = address;
  _wire = &wirePort;
  _accelRange = MPU6050_ACCEL_RANGE_2G;
  _gyroRange  = MPU6050_GYRO_RANGE_250DPS;
}

bool MPU6050::begin(mpu6050_accel_range_t accelRange, mpu6050_gyro_range_t gyroRange, mpu6050_dlpf_t dlpf) {
  _wire->begin();

  if (!isConnected()) {
    return false;
  }

  // Wake the device up (default is asleep after power-on) and select gyro X as clock source
  if (!writeRegister(MPU6050_REG_PWR_MGMT_1, 0x01)) {
    return false;
  }
  delay(50);

  // Make sure both accel and gyro axes are enabled (not in standby)
  writeRegister(MPU6050_REG_PWR_MGMT_2, 0x00);

  setDLPF(dlpf);
  setAccelRange(accelRange);
  setGyroRange(gyroRange);
  setSampleRateDivider(4); // ~200Hz sample rate when DLPF is enabled (1kHz base / (1+4))

  delay(10);
  return true;
}

bool MPU6050::isConnected() {
  uint8_t whoAmI = 0;
  if (!readRegisters(MPU6050_REG_WHO_AM_I, &whoAmI, 1)) {
    return false;
  }
  // Some MPU6050/MPU6500-compatible clones report 0x68, 0x69, 0x70, 0x72, etc.
  // We accept the documented default and treat any non-zero, non-0xFF value as
  // "found a device", but flag the canonical value for strict checks if desired.
  return (whoAmI == MPU6050_WHO_AM_I_VALUE) || (whoAmI != 0x00 && whoAmI != 0xFF);
}

void MPU6050::setAccelRange(mpu6050_accel_range_t range) {
  writeRegister(MPU6050_REG_ACCEL_CONFIG, (uint8_t)range);
  _accelRange = range;
}

void MPU6050::setGyroRange(mpu6050_gyro_range_t range) {
  writeRegister(MPU6050_REG_GYRO_CONFIG, (uint8_t)range);
  _gyroRange = range;
}

void MPU6050::setDLPF(mpu6050_dlpf_t dlpf) {
  writeRegister(MPU6050_REG_CONFIG, (uint8_t)dlpf & 0x07);
}

void MPU6050::setSampleRateDivider(uint8_t divider) {
  writeRegister(MPU6050_REG_SMPLRT_DIV, divider);
}

mpu6050_accel_range_t MPU6050::getAccelRange() {
  return _accelRange;
}

mpu6050_gyro_range_t MPU6050::getGyroRange() {
  return _gyroRange;
}

float MPU6050::accelRangeToLSB(mpu6050_accel_range_t range) {
  switch (range) {
    case MPU6050_ACCEL_RANGE_2G:  return 16384.0f;
    case MPU6050_ACCEL_RANGE_4G:  return 8192.0f;
    case MPU6050_ACCEL_RANGE_8G:  return 4096.0f;
    case MPU6050_ACCEL_RANGE_16G: return 2048.0f;
    default: return 16384.0f;
  }
}

float MPU6050::gyroRangeToLSB(mpu6050_gyro_range_t range) {
  switch (range) {
    case MPU6050_GYRO_RANGE_250DPS:  return 131.0f;
    case MPU6050_GYRO_RANGE_500DPS:  return 65.5f;
    case MPU6050_GYRO_RANGE_1000DPS: return 32.8f;
    case MPU6050_GYRO_RANGE_2000DPS: return 16.4f;
    default: return 131.0f;
  }
}

bool MPU6050::readAll(MPU6050Data &data) {
  uint8_t buf[14];
  if (!readRegisters(MPU6050_REG_ACCEL_XOUT_H, buf, 14)) {
    return false;
  }

  int16_t rawAccelX = (int16_t)((buf[0]  << 8) | buf[1]);
  int16_t rawAccelY = (int16_t)((buf[2]  << 8) | buf[3]);
  int16_t rawAccelZ = (int16_t)((buf[4]  << 8) | buf[5]);
  int16_t rawTemp   = (int16_t)((buf[6]  << 8) | buf[7]);
  int16_t rawGyroX  = (int16_t)((buf[8]  << 8) | buf[9]);
  int16_t rawGyroY  = (int16_t)((buf[10] << 8) | buf[11]);
  int16_t rawGyroZ  = (int16_t)((buf[12] << 8) | buf[13]);

  float accelLSB = accelRangeToLSB(_accelRange);
  float gyroLSB  = gyroRangeToLSB(_gyroRange);

  data.accelX = rawAccelX / accelLSB;
  data.accelY = rawAccelY / accelLSB;
  data.accelZ = rawAccelZ / accelLSB;

  data.gyroX = (rawGyroX / gyroLSB) - _gyroOffsetX;
  data.gyroY = (rawGyroY / gyroLSB) - _gyroOffsetY;
  data.gyroZ = (rawGyroZ / gyroLSB) - _gyroOffsetZ;

  // Per datasheet: Temperature in degrees C = (TEMP_OUT / 340) + 36.53
  data.tempC = (rawTemp / 340.0f) + 36.53f;

  return true;
}

bool MPU6050::readAccel(float &x, float &y, float &z) {
  int16_t rx, ry, rz;
  if (!readRawAccel(rx, ry, rz)) return false;
  float lsb = accelRangeToLSB(_accelRange);
  x = rx / lsb;
  y = ry / lsb;
  z = rz / lsb;
  return true;
}

bool MPU6050::readGyro(float &x, float &y, float &z) {
  int16_t rx, ry, rz;
  if (!readRawGyro(rx, ry, rz)) return false;
  float lsb = gyroRangeToLSB(_gyroRange);
  x = (rx / lsb) - _gyroOffsetX;
  y = (ry / lsb) - _gyroOffsetY;
  z = (rz / lsb) - _gyroOffsetZ;
  return true;
}

bool MPU6050::readTemp(float &tempC) {
  int16_t raw;
  if (!readRawTemp(raw)) return false;
  tempC = (raw / 340.0f) + 36.53f;
  return true;
}

bool MPU6050::readRawAccel(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t buf[6];
  if (!readRegisters(MPU6050_REG_ACCEL_XOUT_H, buf, 6)) return false;
  x = (int16_t)((buf[0] << 8) | buf[1]);
  y = (int16_t)((buf[2] << 8) | buf[3]);
  z = (int16_t)((buf[4] << 8) | buf[5]);
  return true;
}

bool MPU6050::readRawGyro(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t buf[6];
  if (!readRegisters(MPU6050_REG_GYRO_XOUT_H, buf, 6)) return false;
  x = (int16_t)((buf[0] << 8) | buf[1]);
  y = (int16_t)((buf[2] << 8) | buf[3]);
  z = (int16_t)((buf[4] << 8) | buf[5]);
  return true;
}

bool MPU6050::readRawTemp(int16_t &raw) {
  uint8_t buf[2];
  if (!readRegisters(MPU6050_REG_TEMP_OUT_H, buf, 2)) return false;
  raw = (int16_t)((buf[0] << 8) | buf[1]);
  return true;
}

void MPU6050::calibrateGyro(uint16_t samples) {
  double sumX = 0, sumY = 0, sumZ = 0;
  // Reset any existing offsets so raw readings are used during calibration
  _gyroOffsetX = 0;
  _gyroOffsetY = 0;
  _gyroOffsetZ = 0;

  uint16_t validSamples = 0;
  for (uint16_t i = 0; i < samples; i++) {
    float x, y, z;
    if (readGyro(x, y, z)) {
      sumX += x;
      sumY += y;
      sumZ += z;
      validSamples++;
    }
    delay(3);
  }

  if (validSamples > 0) {
    _gyroOffsetX = sumX / validSamples;
    _gyroOffsetY = sumY / validSamples;
    _gyroOffsetZ = sumZ / validSamples;
  }
}

void MPU6050::sleep(bool enable) {
  uint8_t buf;
  if (!readRegisters(MPU6050_REG_PWR_MGMT_1, &buf, 1)) return;
  if (enable) {
    buf |= 0x40;   // set SLEEP bit
  } else {
    buf &= ~0x40;  // clear SLEEP bit
  }
  writeRegister(MPU6050_REG_PWR_MGMT_1, buf);
}

bool MPU6050::writeRegister(uint8_t reg, uint8_t value) {
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _wire->write(value);
  return (_wire->endTransmission() == 0);
}

bool MPU6050::readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length) {
  _wire->beginTransmission(_address);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) {
    return false;
  }

  uint8_t received = _wire->requestFrom(_address, length);
  if (received != length) {
    return false;
  }

  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = _wire->read();
  }
  return true;
}