/*
  MPU6050.h - Driver for GY-521 (MPU-6050) 3-Axis Accelerometer + 3-Axis Gyroscope
  For Arduino Uno R4 WiFi (and other Arduino boards) over I2C (Wire library)

  Wiring (Uno R4 WiFi):
    VCC  -> 5V (or 3.3V, module has onboard regulator on most GY-521 boards)
    GND  -> GND
    SCL  -> SCL (dedicated SCL pin, or A5)
    SDA  -> SDA (dedicated SDA pin, or A4)
    AD0  -> GND (I2C address 0x68) or 5V/3.3V (I2C address 0x69)

  Author: Claude (Anthropic) generated driver
*/

#ifndef MPU6050_H
#define MPU6050_H

#include <Arduino.h>
#include <Wire.h>

// Default I2C address (AD0 pin low). If AD0 is tied high, address becomes 0x69.
#define MPU6050_ADDRESS_AD0_LOW   0x68
#define MPU6050_ADDRESS_AD0_HIGH  0x69

// Register map
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_INT_ENABLE    0x38
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_REG_TEMP_OUT_H    0x41
#define MPU6050_REG_GYRO_XOUT_H   0x43
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_PWR_MGMT_2    0x6C
#define MPU6050_REG_WHO_AM_I      0x75

#define MPU6050_WHO_AM_I_VALUE    0x68

// Accelerometer full scale ranges
enum mpu6050_accel_range_t {
  MPU6050_ACCEL_RANGE_2G  = 0x00,  // +/- 2g
  MPU6050_ACCEL_RANGE_4G  = 0x08,  // +/- 4g
  MPU6050_ACCEL_RANGE_8G  = 0x10,  // +/- 8g
  MPU6050_ACCEL_RANGE_16G = 0x18   // +/- 16g
};

// Gyroscope full scale ranges
enum mpu6050_gyro_range_t {
  MPU6050_GYRO_RANGE_250DPS  = 0x00,  // +/- 250 deg/s
  MPU6050_GYRO_RANGE_500DPS  = 0x08,  // +/- 500 deg/s
  MPU6050_GYRO_RANGE_1000DPS = 0x10,  // +/- 1000 deg/s
  MPU6050_GYRO_RANGE_2000DPS = 0x18   // +/- 2000 deg/s
};

// Digital low pass filter settings (affects bandwidth vs. sample rate)
enum mpu6050_dlpf_t {
  MPU6050_DLPF_260HZ = 0,
  MPU6050_DLPF_184HZ = 1,
  MPU6050_DLPF_94HZ  = 2,
  MPU6050_DLPF_44HZ  = 3,
  MPU6050_DLPF_21HZ  = 4,
  MPU6050_DLPF_10HZ  = 5,
  MPU6050_DLPF_5HZ   = 6
};

// Container for a single reading in physical units
struct MPU6050Data {
  float accelX, accelY, accelZ;  // g
  float gyroX,  gyroY,  gyroZ;   // deg/s
  float tempC;                   // deg C
};

// Vibration statistics finalized at the end of one sample window (see
// updateVibration() below).
struct MPU6050VibrationStats {
  float rmsAccelG;    // RMS of the gravity-removed accel magnitude, in g
  float peakAccelG;   // largest instantaneous gravity-removed magnitude, in g
  float crestFactor;  // peakAccelG / rmsAccelG (0 if rmsAccelG is ~0)
  float tempC;        // sensor temperature, sampled once per window
  uint16_t sampleCount; // number of accel samples folded into this window
};

class MPU6050 {
  public:
    explicit MPU6050(uint8_t address = MPU6050_ADDRESS_AD0_LOW, TwoWire &wirePort = Wire);

    // Initializes I2C, verifies WHO_AM_I, wakes the device, and applies default config.
    // Returns true on success, false if the sensor could not be found/verified.
    bool begin(mpu6050_accel_range_t accelRange = MPU6050_ACCEL_RANGE_2G,
               mpu6050_gyro_range_t gyroRange   = MPU6050_GYRO_RANGE_250DPS,
               mpu6050_dlpf_t dlpf              = MPU6050_DLPF_44HZ);

    bool isConnected();

    void setAccelRange(mpu6050_accel_range_t range);
    void setGyroRange(mpu6050_gyro_range_t range);
    void setDLPF(mpu6050_dlpf_t dlpf);
    void setSampleRateDivider(uint8_t divider); // sample_rate = gyro_rate / (1 + divider)

    mpu6050_accel_range_t getAccelRange();
    mpu6050_gyro_range_t  getGyroRange();

    // Reads all 14 bytes (accel, temp, gyro) in one burst and converts to physical units.
    bool readAll(MPU6050Data &data);

    // Individual reads (each performs its own burst read of the relevant registers)
    bool readAccel(float &x, float &y, float &z);   // g
    bool readGyro(float &x, float &y, float &z);    // deg/s
    bool readTemp(float &tempC);

    // Raw (unscaled) readings, straight from the sensor
    bool readRawAccel(int16_t &x, int16_t &y, int16_t &z);
    bool readRawGyro(int16_t &x, int16_t &y, int16_t &z);
    bool readRawTemp(int16_t &raw);

    // Simple gyro calibration: averages `samples` readings while the sensor is
    // stationary and stores the offsets, which are then subtracted from future
    // readGyro()/readAll() calls. Call after begin(), with the sensor held still.
    void calibrateGyro(uint16_t samples = 500);

    // Puts the device into / wakes it from low-power sleep mode.
    void sleep(bool enable);

    // --- Vibration sample window -----------------------------------------
    // Averages `samples` accel readings to find the static gravity vector.
    // Sensor must be held still, in its resting orientation, while this
    // runs. Call once after begin()/calibrateGyro(), before the main loop.
    void calibrateGravity(uint16_t samples = 500);

    // Sets the polling rate and window length used by updateVibration().
    // Call before beginVibrationWindow(); defaults are 5ms / 500ms if unset.
    void setVibrationWindow(unsigned long sampleIntervalMs, unsigned long windowMs);

    // Resets the sample window's timers/accumulators. Call once, right
    // before the main loop starts calling updateVibration().
    void beginVibrationWindow();

    // Call every loop() iteration. Internally polls accel at the configured
    // sample interval (non-blocking, based on millis()) and folds each
    // reading into the current window's RMS/peak accumulators after
    // subtracting the calibrated gravity vector. Once a full window has
    // elapsed, finalizes the stats into `out`, resets the window, and
    // returns true. Returns false on every other call.
    bool updateVibration(MPU6050VibrationStats &out);

    // Formats `value` as a fixed-point decimal string (e.g. "1.2300"),
    // using only integer math/printf - no "%f", so it works regardless of
    // whether the core's printf was built with float support. Useful when
    // building a JSON/MQTT payload that must never render a whole-number
    // float without a decimal point (which can cause a receiving system to
    // infer an integer type from the first message).
    static void formatFixed(float value, uint8_t decimals, char *buf, size_t bufSize);

  private:
    TwoWire *_wire;
    uint8_t _address;
    mpu6050_accel_range_t _accelRange;
    mpu6050_gyro_range_t  _gyroRange;

    float _gyroOffsetX = 0, _gyroOffsetY = 0, _gyroOffsetZ = 0;

    float _gravityX = 0, _gravityY = 0, _gravityZ = 0;

    unsigned long _sampleIntervalMs = 5;
    unsigned long _windowMs = 500;
    unsigned long _lastSampleTime = 0;
    unsigned long _windowStart = 0;
    double _sumSquares = 0.0;
    float _peakMag = 0.0f;
    uint16_t _sampleCount = 0;

    float accelRangeToLSB(mpu6050_accel_range_t range);
    float gyroRangeToLSB(mpu6050_gyro_range_t range);

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length);
};

#endif // MPU6050_H