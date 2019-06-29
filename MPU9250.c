/******************************************************************************
 These are the base functions for the MPU9250

 Copyright © 2019 eneus (http://eneus.gitub.io/)

 Feel free to do whatever you like with this code.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h> 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <memory.h>
#include <errno.h>
#include <linux/i2c-dev.h>
#include "MPU9250.h"

int initIMU()
{
  // select clock source to gyro
  if(setRegister(PWR_MGMNT_1,CLOCK_SEL_PLL) < 0){
	  printf("ERROR");
    return -1;
  }
  // enable I2C master mode
  if(setRegister(USER_CTRL,I2C_MST_EN) < 0){
	  printf("ERROR");
    return -2;
  }
  // set the I2C bus speed to 400 kHz
  if(setRegister(I2C_MST_CTRL,I2C_MST_CLK) < 0){
	  printf("ERROR");
    return -3;
  }
  // set AK8963 to Power Down
  setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN);
  // reset the MPU9250
  setRegister(PWR_MGMNT_1,PWR_RESET);
  // wait for MPU-9250 to come back up
  usleep(10);
  // reset the AK8963
  setRegistersAK8963(AK8963_CNTL2,AK8963_RESET);
  // select clock source to gyro
  if(setRegister(PWR_MGMNT_1,CLOCK_SEL_PLL) < 0){
    return -4;
  }
  // enable accelerometer and gyro
  if(setRegister(PWR_MGMNT_2,SEN_ENABLE) < 0){
    return -6;
  }
  // setting accel range to 16G as default
  if(setRegister(ACCEL_CONFIG,ACCEL_FS_SEL_16G) < 0){
    return -7;
  }
  _accelScale = G * 16.0f/32767.5f; // setting the accel scale to 16G
  _accelRange = ACCEL_RANGE_16G;
  // setting the gyro range to 2000DPS as default
  if(setRegister(GYRO_CONFIG,GYRO_FS_SEL_2000DPS) < 0){
    return -8;
  }
  _gyroScale = 2000.0f/32767.5f * _d2r; // setting the gyro scale to 2000DPS
  _gyroRange = GYRO_RANGE_2000DPS;
  // setting bandwidth to 184Hz as default
  if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_184) < 0){ 
    return -9;
  } 
  if(setRegister(CONFIG,GYRO_DLPF_184) < 0){ // setting gyro bandwidth to 184Hz
    return -10;
  }
  _bandwidth = DLPF_BANDWIDTH_184HZ;
  // setting the sample rate divider to 0 as default
  if(setRegister(SMPDIV,0x00) < 0){ 
    return -11;
  } 
  _srd = 0;
  // enable I2C master mode
  if(setRegister(USER_CTRL,I2C_MST_EN) < 0){
  	return -12;
  }
  // set the I2C bus speed to 400 kHz
  if(setRegister(I2C_MST_CTRL,I2C_MST_CLK) < 0){
    return -13;
  }
  // check AK8963 WHO AM I register, expected value is 0x48 (decimal 72)
  if(whoAmIAK8963() != 72 ){
    return -14;
  }
  printf("STEEP \n");
  /* get the magnetometer calibration */
  // set AK8963 to Power Down
  if(setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN) < 0){
    return -15;
  }
  usleep(100); // long wait between AK8963 mode changes
  // set AK8963 to FUSE ROM access
  if(setRegistersAK8963(AK8963_CNTL1,AK8963_FUSE_ROM) < 0){
    return -16;
  }
  usleep(100); // long wait between AK8963 mode changes
  // read the AK8963 ASA registers and compute magnetometer scale factors
  getRegistersAK8963(AK8963_ASA,3,_buffer);
  _magScaleX = ((((float)_buffer[0]) - 128.0f)/(256.0f) + 1.0f) * 4912.0f / 32760.0f; // micro Tesla
  _magScaleY = ((((float)_buffer[1]) - 128.0f)/(256.0f) + 1.0f) * 4912.0f / 32760.0f; // micro Tesla
  _magScaleZ = ((((float)_buffer[2]) - 128.0f)/(256.0f) + 1.0f) * 4912.0f / 32760.0f; // micro Tesla 
  // set AK8963 to Power Down
  if(setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN) < 0){
    return -17;
  }
  usleep(100); // long wait between AK8963 mode changes  
  // set AK8963 to 16 bit resolution, 100 Hz update rate
  if(setRegistersAK8963(AK8963_CNTL1,AK8963_CNT_MEAS2) < 0){
    return -18;
  }
  usleep(100); // long wait between AK8963 mode changes
  // select clock source to gyro
  if(setRegister(PWR_MGMNT_1,CLOCK_SEL_PLL) < 0){
    return -19;
  }       
  // instruct the MPU9250 to get 7 bytes of data from the AK8963 at the sample rate
  getRegistersAK8963(AK8963_HXL,7,_buffer);
  // estimate gyro bias
  if (calibrateGyro() < 0) {
    return -20;
  }
  // successful init, return 1

  return 1;
}

/* sets the accelerometer full scale range to values other than default */
int setAccelRange(char range) {
  switch(range) {
    case ACCEL_RANGE_2G: {
      // setting the accel range to 2G
      if(setRegister(ACCEL_CONFIG,ACCEL_FS_SEL_2G) < 0){
        return -1;
      }
      _accelScale = G * 2.0f/32767.5f; // setting the accel scale to 2G
      break; 
    }
    case ACCEL_RANGE_4G: {
      // setting the accel range to 4G
      if(setRegister(ACCEL_CONFIG,ACCEL_FS_SEL_4G) < 0){
        return -1;
      }
      _accelScale = G * 4.0f/32767.5f; // setting the accel scale to 4G
      break;
    }
    case ACCEL_RANGE_8G: {
      // setting the accel range to 8G
      if(setRegister(ACCEL_CONFIG,ACCEL_FS_SEL_8G) < 0){
        return -1;
      }
      _accelScale = G * 8.0f/32767.5f; // setting the accel scale to 8G
      break;
    }
    case ACCEL_RANGE_16G: {
      // setting the accel range to 16G
      if(setRegister(ACCEL_CONFIG,ACCEL_FS_SEL_16G) < 0){
        return -1;
      }
      _accelScale = G * 16.0f/32767.5f; // setting the accel scale to 16G
      break;
    }
  }
  _accelRange = range;
  return 1;
}

/* sets the gyro full scale range to values other than default */
int setGyroRange(char range)
{
  switch(range) {
    case GYRO_RANGE_250DPS: {
      // setting the gyro range to 250DPS
      if(setRegister(GYRO_CONFIG,GYRO_FS_SEL_250DPS) < 0){
        return -1;
      }
      _gyroScale = 250.0f/32767.5f * _d2r; // setting the gyro scale to 250DPS
      break;
    }
    case GYRO_RANGE_500DPS: {
      // setting the gyro range to 500DPS
      if(setRegister(GYRO_CONFIG,GYRO_FS_SEL_500DPS) < 0){
        return -1;
      }
      _gyroScale = 500.0f/32767.5f * _d2r; // setting the gyro scale to 500DPS
      break;  
    }
    case GYRO_RANGE_1000DPS: {
      // setting the gyro range to 1000DPS
      if(setRegister(GYRO_CONFIG,GYRO_FS_SEL_1000DPS) < 0){
        return -1;
      }
      _gyroScale = 1000.0f/32767.5f * _d2r; // setting the gyro scale to 1000DPS
      break;
    }
    case GYRO_RANGE_2000DPS: {
      // setting the gyro range to 2000DPS
      if(setRegister(GYRO_CONFIG,GYRO_FS_SEL_2000DPS) < 0){
        return -1;
      }
      _gyroScale = 2000.0f/32767.5f * _d2r; // setting the gyro scale to 2000DPS
      break;
    }
  }
  _gyroRange = range;
  return 1;
}

/* sets the DLPF bandwidth to values other than default */
int setDlpfBandwidth(char bandwidth)
{
  switch(bandwidth) {
    case DLPF_BANDWIDTH_184HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_184) < 0){ // setting accel bandwidth to 184Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_184) < 0){ // setting gyro bandwidth to 184Hz
        return -2;
      }
      break;
    }
    case DLPF_BANDWIDTH_92HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_92) < 0){ // setting accel bandwidth to 92Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_92) < 0){ // setting gyro bandwidth to 92Hz
        return -2;
      }
      break;
    }
    case DLPF_BANDWIDTH_41HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_41) < 0){ // setting accel bandwidth to 41Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_41) < 0){ // setting gyro bandwidth to 41Hz
        return -2;
      }
      break;
    }
    case DLPF_BANDWIDTH_20HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_20) < 0){ // setting accel bandwidth to 20Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_20) < 0){ // setting gyro bandwidth to 20Hz
        return -2;
      }
      break;
    }
    case DLPF_BANDWIDTH_10HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_10) < 0){ // setting accel bandwidth to 10Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_10) < 0){ // setting gyro bandwidth to 10Hz
        return -2;
      }
      break;
    }
    case DLPF_BANDWIDTH_5HZ: {
      if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_5) < 0){ // setting accel bandwidth to 5Hz
        return -1;
      } 
      if(setRegister(CONFIG,GYRO_DLPF_5) < 0){ // setting gyro bandwidth to 5Hz
        return -2;
      }
      break;
    }
  }
  _bandwidth = bandwidth;
  return 1;
}

/* sets the sample rate divider to values other than default */
int setSrd(uint8_t srd) {
  /* setting the sample rate divider to 19 to facilitate setting up magnetometer */
  if(setRegister(SMPDIV,19) < 0){ // setting the sample rate divider
    return -1;
  }
  if(srd > 9){
    // set AK8963 to Power Down
    if(setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN) < 0){
      return -2;
    }
    usleep(100); // long wait between AK8963 mode changes  
    // set AK8963 to 16 bit resolution, 8 Hz update rate
    if(setRegistersAK8963(AK8963_CNTL1,AK8963_CNT_MEAS1) < 0){
      return -3;
    }
    usleep(100); // long wait between AK8963 mode changes     
    // instruct the MPU9250 to get 7 bytes of data from the AK8963 at the sample rate
    getRegistersAK8963(AK8963_HXL,7,_buffer);
  } else {
    // set AK8963 to Power Down
    if(setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN) < 0){
      return -2;
    }
    usleep(100); // long wait between AK8963 mode changes  
    // set AK8963 to 16 bit resolution, 100 Hz update rate
    if(setRegistersAK8963(AK8963_CNTL1,AK8963_CNT_MEAS2) < 0){
      return -3;
    }
    usleep(100); // long wait between AK8963 mode changes     
    // instruct the MPU9250 to get 7 bytes of data from the AK8963 at the sample rate
    getRegistersAK8963(AK8963_HXL,7,_buffer);    
  } 
  /* setting the sample rate divider */
  if(setRegister(SMPDIV,srd) < 0){ // setting the sample rate divider
    return -4;
  } 
  _srd = srd;
  return 1; 
}

/* enables the data ready interrupt */
int enableDataReadyInterrupt() {
  /* setting the interrupt */
  if (setRegister(INT_PIN_CFG,INT_PULSE_50US) < 0){ // setup interrupt, 50 us pulse
    return -1;
  }  
  if (setRegister(INT_ENABLE,INT_RAW_RDY_EN) < 0){ // set to data ready
    return -2;
  }
  return 1;
}

/* disables the data ready interrupt */
int disableDataReadyInterrupt() {
  if(setRegister(INT_ENABLE,INT_DISABLE) < 0){ // disable interrupt
    return -1;
  }  
  return 1;
}

/* configures and enables wake on motion, low power mode */
int enableWakeOnMotion(float womThresh_mg, int odr) {
  // set AK8963 to Power Down
  setRegistersAK8963(AK8963_CNTL1,AK8963_PWR_DOWN);
  // reset the MPU9250
  setRegister(PWR_MGMNT_1,PWR_RESET);
  // wait for MPU-9250 to come back up
  usleep(1);
  if(setRegister(PWR_MGMNT_1,0x00) < 0){ // cycle 0, sleep 0, standby 0
    return -1;
  } 
  if(setRegister(PWR_MGMNT_2,DIS_GYRO) < 0){ // disable gyro measurements
    return -2;
  } 
  if(setRegister(ACCEL_CONFIG2,ACCEL_DLPF_184) < 0){ // setting accel bandwidth to 184Hz
    return -3;
  } 
  if(setRegister(INT_ENABLE,INT_WOM_EN) < 0){ // enabling interrupt to wake on motion
    return -4;
  } 
  if(setRegister(MOT_DETECT_CTRL,(ACCEL_INTEL_EN | ACCEL_INTEL_MODE)) < 0){ // enabling accel hardware intelligence
    return -5;
  } 
//   _womThreshold = map(womThresh_mg, 0, 1020, 0, 255);
//   if(setRegister(WOM_THR,_womThreshold) < 0){ // setting wake on motion threshold
//     return -6;
//   }
  if(setRegister(LP_ACCEL_ODR,(uint8_t)odr) < 0){ // set frequency of wakeup
    return -7;
  }
  if(setRegister(PWR_MGMNT_1,PWR_CYCLE) < 0){ // switch to accel low power mode
    return -8;
  }
  return 1;
}

/* reads the most current data from MPU9250 and stores in buffer */
int readSensor() {
  // grab the data from the MPU9250
  if (getRegisters(ACCEL_OUT, 21, _buffer) < 0) {
    return -1;
  }
  // combine into 16 bit values
  _axcounts = (((int16_t)_buffer[0]) << 8) | _buffer[1];  
  _aycounts = (((int16_t)_buffer[2]) << 8) | _buffer[3];
  _azcounts = (((int16_t)_buffer[4]) << 8) | _buffer[5];
  _tcounts = (((int16_t)_buffer[6]) << 8) | _buffer[7];
  _gxcounts = (((int16_t)_buffer[8]) << 8) | _buffer[9];
  _gycounts = (((int16_t)_buffer[10]) << 8) | _buffer[11];
  _gzcounts = (((int16_t)_buffer[12]) << 8) | _buffer[13];
  _hxcounts = (((int16_t)_buffer[15]) << 8) | _buffer[14];
  _hycounts = (((int16_t)_buffer[17]) << 8) | _buffer[16];
  _hzcounts = (((int16_t)_buffer[19]) << 8) | _buffer[18];
  // transform and convert to float values
  _ax = (((float)(tX[0]*_axcounts + tX[1]*_aycounts + tX[2]*_azcounts) * _accelScale) - _axb)*_axs;
  _ay = (((float)(tY[0]*_axcounts + tY[1]*_aycounts + tY[2]*_azcounts) * _accelScale) - _ayb)*_ays;
  _az = (((float)(tZ[0]*_axcounts + tZ[1]*_aycounts + tZ[2]*_azcounts) * _accelScale) - _azb)*_azs;
  _gx = ((float)(tX[0]*_gxcounts + tX[1]*_gycounts + tX[2]*_gzcounts) * _gyroScale) - _gxb;
  _gy = ((float)(tY[0]*_gxcounts + tY[1]*_gycounts + tY[2]*_gzcounts) * _gyroScale) - _gyb;
  _gz = ((float)(tZ[0]*_gxcounts + tZ[1]*_gycounts + tZ[2]*_gzcounts) * _gyroScale) - _gzb;
  _hx = (((float)(_hxcounts) * _magScaleX) - _hxb)*_hxs;
  _hy = (((float)(_hycounts) * _magScaleY) - _hyb)*_hys;
  _hz = (((float)(_hzcounts) * _magScaleZ) - _hzb)*_hzs;
  _t = ((((float) _tcounts) - _tempOffset)/_tempScale) + _tempOffset;
  return 1;
}

/* returns the accelerometer measurement in the x direction, m/s/s */
float getAccelX_mss() {
  return _ax;
}

/* returns the accelerometer measurement in the y direction, m/s/s */
float getAccelY_mss() {
  return _ay;
}

/* returns the accelerometer measurement in the z direction, m/s/s */
float getAccelZ_mss() {
  return _az;
}

/* returns the gyroscope measurement in the x direction, rad/s */
float getGyroX_rads() {
  return _gx;
}

/* returns the gyroscope measurement in the y direction, rad/s */
float getGyroY_rads() {
  return _gy;
}

/* returns the gyroscope measurement in the z direction, rad/s */
float getGyroZ_rads() {
  return _gz;
}

/* returns the magnetometer measurement in the x direction, uT */
float getMagX_uT() {
  return _hx;
}

/* returns the magnetometer measurement in the y direction, uT */
float getMagY_uT() {
  return _hy;
}

/* returns the magnetometer measurement in the z direction, uT */
float getMagZ_uT() {
  return _hz;
}

/* returns the die temperature, C */
float getTemperature_C() {
  return _t;
}

/* estimates the gyro biases */
int calibrateGyro() {
  // set the range, bandwidth, and srd
  if (setGyroRange(GYRO_RANGE_250DPS) < 0) {
    return -1;
  }
  if (setDlpfBandwidth(DLPF_BANDWIDTH_20HZ) < 0) {
    return -2;
  }
  if (setSrd(19) < 0) {
    return -3;
  }

  // take samples and find bias
  _gxbD = 0;
  _gybD = 0;
  _gzbD = 0;
  for (size_t i=0; i < _numSamples; i++) {
    readSensor();
    _gxbD += (getGyroX_rads() + _gxb)/((double)_numSamples);
    _gybD += (getGyroY_rads() + _gyb)/((double)_numSamples);
    _gzbD += (getGyroZ_rads() + _gzb)/((double)_numSamples);
    usleep(20);
  }
  _gxb = (float)_gxbD;
  _gyb = (float)_gybD;
  _gzb = (float)_gzbD;

  // set the range, bandwidth, and srd back to what they were
  if (setGyroRange(_gyroRange) < 0) {
    return -4;
  }
  if (setDlpfBandwidth(_bandwidth) < 0) {
    return -5;
  }
  if (setSrd(_srd) < 0) {
    return -6;
  }
  return 1;
}

/* returns the gyro bias in the X direction, rad/s */
float getGyroBiasX_rads() {
  return _gxb;
}

/* returns the gyro bias in the Y direction, rad/s */
float getGyroBiasY_rads() {
  return _gyb;
}

/* returns the gyro bias in the Z direction, rad/s */
float getGyroBiasZ_rads() {
  return _gzb;
}

/* sets the gyro bias in the X direction to bias, rad/s */
void setGyroBiasX_rads(float bias) {
  _gxb = bias;
}

/* sets the gyro bias in the Y direction to bias, rad/s */
void setGyroBiasY_rads(float bias) {
  _gyb = bias;
}

/* sets the gyro bias in the Z direction to bias, rad/s */
void setGyroBiasZ_rads(float bias) {
  _gzb = bias;
}

/* finds bias and scale factor calibration for the accelerometer,
this should be run for each axis in each direction (6 total) to find
the min and max values along each */
int calibrateAccel() {
  // set the range, bandwidth, and srd
  if (setAccelRange(ACCEL_RANGE_2G) < 0) {
    return -1;
  }
  if (setDlpfBandwidth(DLPF_BANDWIDTH_20HZ) < 0) {
    return -2;
  }
  if (setSrd(19) < 0) {
    return -3;
  }

  // take samples and find min / max 
  _axbD = 0;
  _aybD = 0;
  _azbD = 0;
  for (size_t i=0; i < _numSamples; i++) {
    readSensor();
    _axbD += (getAccelX_mss()/_axs + _axb)/((double)_numSamples);
    _aybD += (getAccelY_mss()/_ays + _ayb)/((double)_numSamples);
    _azbD += (getAccelZ_mss()/_azs + _azb)/((double)_numSamples);
    usleep(20);
  }
  if (_axbD > 9.0f) {
    _axmax = (float)_axbD;
  }
  if (_aybD > 9.0f) {
    _aymax = (float)_aybD;
  }
  if (_azbD > 9.0f) {
    _azmax = (float)_azbD;
  }
  if (_axbD < -9.0f) {
    _axmin = (float)_axbD;
  }
  if (_aybD < -9.0f) {
    _aymin = (float)_aybD;
  }
  if (_azbD < -9.0f) {
    _azmin = (float)_azbD;
  }

  // find bias and scale factor
  if ((abs(_axmin) > 9.0f) && (abs(_axmax) > 9.0f)) {
    _axb = (_axmin + _axmax) / 2.0f;
    _axs = G/((abs(_axmin) + abs(_axmax)) / 2.0f);
  }
  if ((abs(_aymin) > 9.0f) && (abs(_aymax) > 9.0f)) {
    _ayb = (_aymin + _aymax) / 2.0f;
    _ays = G/((abs(_aymin) + abs(_aymax)) / 2.0f);
  }
  if ((abs(_azmin) > 9.0f) && (abs(_azmax) > 9.0f)) {
    _azb = (_azmin + _azmax) / 2.0f;
    _azs = G/((abs(_azmin) + abs(_azmax)) / 2.0f);
  }

  // set the range, bandwidth, and srd back to what they were
  if (setAccelRange(_accelRange) < 0) {
    return -4;
  }
  if (setDlpfBandwidth(_bandwidth) < 0) {
    return -5;
  }
  if (setSrd(_srd) < 0) {
    return -6;
  }
  return 1;  
}

/* returns the accelerometer bias in the X direction, m/s/s */
float getAccelBiasX_mss() {
  return _axb;
}

/* returns the accelerometer scale factor in the X direction */
float getAccelScaleFactorX() {
  return _axs;
}

/* returns the accelerometer bias in the Y direction, m/s/s */
float getAccelBiasY_mss() {
  return _ayb;
}

/* returns the accelerometer scale factor in the Y direction */
float getAccelScaleFactorY() {
  return _ays;
}

/* returns the accelerometer bias in the Z direction, m/s/s */
float getAccelBiasZ_mss() {
  return _azb;
}

/* returns the accelerometer scale factor in the Z direction */
float getAccelScaleFactorZ() {
  return _azs;
}

/* sets the accelerometer bias (m/s/s) and scale factor in the X direction */
void setAccelCalX(float bias,float scaleFactor) {
  _axb = bias;
  _axs = scaleFactor;
}

/* sets the accelerometer bias (m/s/s) and scale factor in the Y direction */
void setAccelCalY(float bias,float scaleFactor) {
  _ayb = bias;
  _ays = scaleFactor;
}

/* sets the accelerometer bias (m/s/s) and scale factor in the Z direction */
void setAccelCalZ(float bias,float scaleFactor) {
  _azb = bias;
  _azs = scaleFactor;
}

/* finds bias and scale factor calibration for the magnetometer,
the sensor should be rotated in a figure 8 motion until complete */
int calibrateMag() {
  // set the srd
  if (setSrd(19) < 0) {
    return -1;
  }

  // get a starting set of data
  readSensor();
  _hxmax = getMagX_uT();
  _hxmin = getMagX_uT();
  _hymax = getMagY_uT();
  _hymin = getMagY_uT();
  _hzmax = getMagZ_uT();
  _hzmin = getMagZ_uT();

  // collect data to find max / min in each channel
  _counter = 0;
  while (_counter < _maxCounts) {
    _delta = 0.0f;
    _framedelta = 0.0f;
    readSensor();
    _hxfilt = (_hxfilt*((float)_coeff-1)+(getMagX_uT()/_hxs+_hxb))/((float)_coeff);
    _hyfilt = (_hyfilt*((float)_coeff-1)+(getMagY_uT()/_hys+_hyb))/((float)_coeff);
    _hzfilt = (_hzfilt*((float)_coeff-1)+(getMagZ_uT()/_hzs+_hzb))/((float)_coeff);
    if (_hxfilt > _hxmax) {
      _delta = _hxfilt - _hxmax;
      _hxmax = _hxfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_hyfilt > _hymax) {
      _delta = _hyfilt - _hymax;
      _hymax = _hyfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_hzfilt > _hzmax) {
      _delta = _hzfilt - _hzmax;
      _hzmax = _hzfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_hxfilt < _hxmin) {
      _delta = abs(_hxfilt - _hxmin);
      _hxmin = _hxfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_hyfilt < _hymin) {
      _delta = abs(_hyfilt - _hymin);
      _hymin = _hyfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_hzfilt < _hzmin) {
      _delta = abs(_hzfilt - _hzmin);
      _hzmin = _hzfilt;
    }
    if (_delta > _framedelta) {
      _framedelta = _delta;
    }
    if (_framedelta > _deltaThresh) {
      _counter = 0;
    } else {
      _counter++;
    }
    usleep(20);
  }

  // find the magnetometer bias
  _hxb = (_hxmax + _hxmin) / 2.0f;
  _hyb = (_hymax + _hymin) / 2.0f;
  _hzb = (_hzmax + _hzmin) / 2.0f;

  // find the magnetometer scale factor
  _hxs = (_hxmax - _hxmin) / 2.0f;
  _hys = (_hymax - _hymin) / 2.0f;
  _hzs = (_hzmax - _hzmin) / 2.0f;
  _avgs = (_hxs + _hys + _hzs) / 3.0f;
  _hxs = _avgs/_hxs;
  _hys = _avgs/_hys;
  _hzs = _avgs/_hzs;

  // set the srd back to what it was
  if (setSrd(_srd) < 0) {
    return -2;
  }
  return 1;
}

/* returns the magnetometer bias in the X direction, uT */
float getMagBiasX_uT() {
  return _hxb;
}

/* returns the magnetometer scale factor in the X direction */
float getMagScaleFactorX() {
  return _hxs;
}

/* returns the magnetometer bias in the Y direction, uT */
float getMagBiasY_uT() {
  return _hyb;
}

/* returns the magnetometer scale factor in the Y direction */
float getMagScaleFactorY() {
  return _hys;
}

/* returns the magnetometer bias in the Z direction, uT */
float getMagBiasZ_uT() {
  return _hzb;
}

/* returns the magnetometer scale factor in the Z direction */
float getMagScaleFactorZ() {
  return _hzs;
}

/* sets the magnetometer bias (uT) and scale factor in the X direction */
void setMagCalX(float bias,float scaleFactor) {
  _hxb = bias;
  _hxs = scaleFactor;
}

/* sets the magnetometer bias (uT) and scale factor in the Y direction */
void setMagCalY(float bias,float scaleFactor) {
  _hyb = bias;
  _hys = scaleFactor;
}

/* sets the magnetometer bias (uT) and scale factor in the Z direction */
void setMagCalZ(float bias,float scaleFactor) {
  _hzb = bias;
  _hzs = scaleFactor;
}

/* writes a register to the AK8963 given a register address and data */
int setRegistersAK8963(uint8_t subAddress, uint8_t data)
{
  // set slave 0 to the AK8963 and set for write
	if (setRegister(I2C_SLV0_ADDR,AK8963_I2C_ADDR) < 0) {
    return -1;
  }
  // set the register to the desired AK8963 sub address 
	if (setRegister(I2C_SLV0_REG,subAddress) < 0) {
    return -2;
  }
  // store the data for write
	if (setRegister(I2C_SLV0_DO,data) < 0) {
    return -3;
  }
  // enable I2C and send 1 byte
	if (setRegister(I2C_SLV0_CTRL,I2C_SLV0_EN | (uint8_t)1) < 0) {
    return -4;
  }
	// read the register and confirm
	if (getRegistersAK8963(subAddress,1,_buffer) < 0) {
    return -5;
  }
	if(_buffer[0] == data) {
  	return 1;
  } else{
  	return -6;
  }
}

/* reads registers from the AK8963 */
int getRegistersAK8963(uint8_t subAddress, uint8_t count, uint8_t* dest)
{
  // set slave 0 to the AK8963 and set for read
	if (setRegister(I2C_SLV0_ADDR,AK8963_I2C_ADDR | I2C_READ_FLAG) < 0) {
    return -1;
  }
  // set the register to the desired AK8963 sub address
	if (setRegister(I2C_SLV0_REG,subAddress) < 0) {
    return -2;
  }
  // enable I2C and request the bytes
	if (setRegister(I2C_SLV0_CTRL,I2C_SLV0_EN | count) < 0) {
    return -3;
  }
	usleep(1); // takes some time for these registers to fill
  // read the bytes off the MPU9250 EXT_SENS_DATA registers
	_status = getRegisters(EXT_SENS_DATA_00,count,dest); 
  return _status;
}

/* gets the MPU9250 WHO_AM_I register value, expected to be 0x71 */
int whoAmI(){
  // read the WHO AM I register
  if (getRegisters(WHO_AM_I,1,_buffer) < 0) {
    return -1;
  }
  // return the register value
  return _buffer[0];
}

/* gets the AK8963 WHO_AM_I register value, expected to be 0x48 */
int whoAmIAK8963(){
  // read the WHO AM I register
  if (getRegistersAK8963(AK8963_WHO_AM_I,1,_buffer) < 0) {
    return -1;
  }
  // return the register value
  return _buffer[0];
}

// Select Device
void detectIMU() 
{
	char filename[20];
	unsigned char regAddr = 0x01;

	sprintf(filename, "/dev/i2c-%d", 1);
	file = open(filename, O_RDWR);

	if (file < 0) {
		printf("Unable to open I2C bus!");
        exit(-1);
	}
	if (ioctl(file, I2C_SLAVE, MPU9250_ADDRESS) < 0) {
		printf("Failed to select I2C device.");
		exit(-1);
	}
	if (write(file, &regAddr, 1) != 1) {
		printf("Failed to write device(%d)\n", MPU9250_ADDRESS);
		exit(-1);
	}
	int response = i2c_smbus_read_byte_data(file, WHO_AM_I);
	// check the WHO AM I byte, expected value is 0x71 (decimal 113) or 0x73 (decimal 115)
	if (response == 113 || response == 115 ){
		printf ("## MPU9250 DETECTED or similar ## \n Addr is %#02x (decimal %d)\n", response, response);
	} else {
		printf ("NO IMU DETECTED\n");
		exit(-1);
	}
    close(file);
}

/**
 * write a byte into the register on a i2c device
 *
 * @param regAddr address of register
 * @param data one bytes
 * @return success or failure
 *
 */
bool setRegister(unsigned char regAddr, unsigned char data) {
	return setRegisters(regAddr, 1, &data);
}

/**
 * write serveral bytes into the register on a i2c device
 *
 * @param regAddr address of register
 * @param length length of data 
 * @param data serveral bits
 * @return success or failure
 *
 */
bool setRegisters(unsigned char regAddr, unsigned char length, unsigned char * data) {

	char count = 0;
	unsigned char buf[128];
	char filename[20];
	
	bool result = true;

	if (length > 127) {
		printf("length (%d) > 127\n", length);
		result = false;
		goto Exit;
	}

	sprintf(filename, "/dev/i2c-%d", 1);
	file = open(filename, O_RDWR);

	if (file < 0) {
		printf("Unable to open I2C bus!");
        exit(-1);
	}
	if (ioctl(file, I2C_SLAVE, MPU9250_ADDRESS) < 0) {
		printf("Failed to select I2C device.");
		exit(-1);
	}

	buf[0] = regAddr;
	memcpy(buf + 1, data, length);
	count = write(file, buf, length + 1);
	if (count < 0) {
		printf("Failed to write device(%d)\n", MPU9250_ADDRESS);
		result = false;
		goto Exit;
	} else if (count != length + 1) {
		printf("Short write to device, expected %d, got %d\n", length + 1, count);
		result = false;
		goto Exit;
	}

	goto Exit;

	Exit:
	close(file);
	return result;
}

/**
 * read a byte from the register on a i2c device
 *
 * @param regAddr address of register
 * @param data a byte
 * @return data length
 *
 */
char getRegister(unsigned char regAddr, unsigned char *data) {
	return getRegisters(regAddr, 1, data);
}

/**
 * read serveral bytes from the register on a i2c device
 *
 * @param regAddr address of register
 * @param length length of data
 * @param data a byte
 * @return data length
 *
 */
char getRegisters(unsigned char regAddr, unsigned char length, unsigned char *data) {

	char count = 0;
	char filename[20];

	sprintf(filename, "/dev/i2c-%d", 1);
	file = open(filename, O_RDWR);

	if (file < 0) {
		printf("Unable to open I2C bus!");
        exit(-1);
	}
	if (ioctl(file, I2C_SLAVE, MPU9250_ADDRESS) < 0) {
		printf("Failed to select I2C device.");
		exit(-1);
	}
	if (write(file, &regAddr, 1) != 1) {
		printf("Failed to write reg: \n");
		count = -1;
		goto Exit;
	}
	count = read(file, data, length);
	if (count < 0) {
		printf("Failed to read device(%d): \n", count);
		count = -1;
		goto Exit;
	} else if (count != length) {
		printf("Short read  from device, expected %d, got %d\n", length, count);
		count = -1;
		goto Exit;
	}

	goto Exit;

	Exit:
	close(file);
	return count;
}
