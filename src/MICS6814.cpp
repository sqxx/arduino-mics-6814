#include "MICS6814.h"

MICS6814::MICS6814(int CO, int NO2, int NH3) {
    _pinCO  = CO;
    _pinNO2 = NO2;
    _pinNH3 = NH3;
}

void MICS6814::calibrate() {
    // Continuously measure the resistance,
    // storing the last N measurements in a circular buffer.
    // Calculate the floating average of the last seconds.
    // If the current measurement is close to the average stop.

    // Seconds to keep stable for successful calibration
    // (Keeps smaller than 64 to prevent overflows)
    uint8_t seconds = 10;

    // Allowed delta for the average from the current value
    uint8_t delta = 2;

    // Circular buffer for the measurements
    uint16_t bufferNH3[seconds];
    uint16_t bufferRED[seconds];
    uint16_t bufferOX[seconds];

    // Pointers for the next element in the buffer
    uint8_t pntrNH3 = 0;
    uint8_t pntrRED = 0;
    uint8_t pntrOX = 0;

    // Current floating sum in the buffer
    uint16_t fltSumNH3 = 0;
    uint16_t fltSumRED = 0;
    uint16_t fltSumOX = 0;

    // Current measurements;
    uint16_t curNH3;
    uint16_t curRED;
    uint16_t curOX;

    // Flag to see if the channels are stable
    bool NH3stable = false;
    bool REDstable = false;
    bool OXstable = false;

    // Initialize buffer
    for (int i = 0; i < seconds; ++i)
    {
        bufferNH3[i] = 0;
        bufferRED[i] = 0;
        bufferOX[i] = 0;
    }

    do
    {
        // Wait a second
        delay(1000);

        // Read new resistances
        unsigned long rs = 0;

        delay(50);

        for (int i = 0; i < 3; i++)
        {
            delay(1);
            rs += analogRead(_pinNH3);
        }

        curNH3 = rs / 3;
        rs = 0;

        delay(50);

        for (int i = 0; i < 3; i++)
        {
            delay(1);
            rs += analogRead(_pinCO);
        }

        curRED = rs / 3;
        rs = 0;

        delay(50);

        for (int i = 0; i < 3; i++)
        {
            delay(1);
            rs += analogRead(_pinNO2);
        }

        curOX = rs / 3;

        // Update floating sum by subtracting value
        // about to be overwritten and adding the new value.
        fltSumNH3 = fltSumNH3 + curNH3 - bufferNH3[pntrNH3];
        fltSumRED = fltSumRED + curRED - bufferRED[pntrRED];
        fltSumOX = fltSumOX + curOX - bufferOX[pntrOX];

        // Store new measurement in buffer
        bufferNH3[pntrNH3] = curNH3;
        bufferRED[pntrRED] = curRED;
        bufferOX[pntrOX] = curOX;

        // Determine new state of flags
        NH3stable = abs(fltSumNH3 / seconds - curNH3) < delta;
        REDstable = abs(fltSumRED / seconds - curRED) < delta;
        OXstable = abs(fltSumOX / seconds - curOX) < delta;

        // Advance buffer pointer
        pntrNH3 = (pntrNH3 + 1) % seconds;
        pntrRED = (pntrRED + 1) % seconds;
        pntrOX = (pntrOX + 1) % seconds;
    } while (!NH3stable || !REDstable || !OXstable);

    NH3baseR = fltSumNH3 / seconds;
    REDbaseR = fltSumRED / seconds;
    OXbaseR = fltSumOX / seconds;
}

void MICS6814::loadCalibrationData(
    uint16_t base_NH3,
    uint16_t base_RED,
    uint16_t base_OX) {
    NH3baseR = base_NH3;
    REDbaseR = base_RED;
    OXbaseR = base_OX;
}

float MICS6814::measure(gas_t gas)
{
  float ratio;
  float c = 0;

  switch (gas)
  {
  case CO:
    ratio = getCurrentRatio(CH_RED);
    c = pow(ratio, -1.179) * 4.385;
    break;
  case NO2:
    ratio = getCurrentRatio(CH_OX);
    c = pow(ratio, 1.007) / 6.855;
    break;
  case NH3:
    ratio = getCurrentRatio(CH_NH3);
    c = pow(ratio, -1.67) / 1.47;
    break;
  }

  return isnan(c) ? -1 : c;
}

uint16_t MICS6814::getResistance(channel_t channel) const {
    unsigned long rs = 0;
    int counter = 0;

    switch (channel)
    {
    case CH_NH3:
        for (int i = 0; i < 100; i++)
        {
            rs += analogRead(_pinNH3);
            counter++;
            delay(2);
        }
        return rs / counter;
    case CH_RED:
        for (int i = 0; i < 100; i++)
        {
            rs += analogRead(_pinCO);
            counter++;
            delay(2);
        }
        return rs / counter;
    case CH_OX:
        for (int i = 0; i < 100; i++)
        {
            rs += analogRead(_pinNO2);
            counter++;
            delay(2);
        }
        return rs / counter;
  }

  return 0;
}

uint16_t MICS6814::getBaseResistance(channel_t channel) const {
    switch (channel)
    {
    case CH_NH3:
        return NH3baseR;
    case CH_RED:
        return REDbaseR;
    case CH_OX:
        return OXbaseR;
    }

    return 0;
}

float MICS6814::getCurrentRatio(channel_t channel) const
{
  float baseResistance = (float)getBaseResistance(channel);
  float resistance = (float)getResistance(channel);

  return resistance / baseResistance * (1023.0 - baseResistance) / (1023.0 - resistance);

  return -1.0;
}