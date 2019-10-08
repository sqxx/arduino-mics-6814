#include "MICS6814.h"

MICS6814::MICS6814(int pinCO, int pinNO2, int pinNH3)
{
	_pinCO  = pinCO;
	_pinNO2 = pinNO2;
	_pinNH3 = pinNH3;
}

/**
 * Калибрует MICS-6814 перед использованием
 *
 * Алгоритм работы:
 *
 * Непрерывно измеряет сопротивление,
 *   с сохранением последних N измерений в буфере.
 * Вычисляет плавающее среднее за последние секунды.
 * Если текущее измерение близко к среднему,
 *   то считаем, что каллибровка прошла успешно.
 */
void MICS6814::calibrate()
{
	// Кол-во секунд, которые должны пройти прежде,
	//   чем мы будем считать, что каллибровка завершена
	// (Меньше 64 секунд, чтобы избежать переполнения)
	uint8_t seconds = 10;

	// Допустимое отклонение для среднего от текущего значения
	uint8_t delta = 2;

	// Буферы измерений
	uint16_t bufferNH3[seconds];
	uint16_t bufferCO[seconds];
	uint16_t bufferNO2[seconds];

	// Указатели для следующего элемента в буфере
	uint8_t pntrNH3 = 0;
	uint8_t pntrCO = 0;
	uint8_t pntrNO2 = 0;

	// Текущая плавающая сумма в буфере
	uint16_t fltSumNH3 = 0;
	uint16_t fltSumCO = 0;
	uint16_t fltSumNO2 = 0;

	// Текущее измерение
	uint16_t curNH3;
	uint16_t curCO;
	uint16_t curNO2;

	// Флаг стабильности показаний
	bool isStableNH3 = false;
	bool isStableCO  = false;
	bool isStableNO2 = false;

	// Забиваем буфер нулями
	for (int i = 0; i < seconds; ++i)
	{
		bufferNH3[i] = 0;
		bufferCO[i]  = 0;
		bufferNO2[i] = 0;
	}

	// Калибруем
	do
	{
		delay(1000);

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

		curCO = rs / 3;
		rs = 0;

		delay(50);
		for (int i = 0; i < 3; i++)
		{
			delay(1);
			rs += analogRead(_pinNO2);
		}

		curNO2 = rs / 3;

		// Обновите плавающую сумму, вычитая значение, 
		//   которое будет перезаписано, и добавив новое значение.
		fltSumNH3 = fltSumNH3 + curNH3 - bufferNH3[pntrNH3];
		fltSumCO  = fltSumCO  + curCO  - bufferCO[pntrCO];
		fltSumNO2 = fltSumNO2 + curNO2 - bufferNO2[pntrNO2];

		// Сохраняем d буфере новые значения
		bufferNH3[pntrNH3] = curNH3;
		bufferCO[pntrCO]   = curCO;
		bufferNO2[pntrNO2] = curNO2; 

		// Определение состояний флагов
		isStableNH3 = abs(fltSumNH3 / seconds - curNH3) < delta;
		isStableCO  = abs(fltSumCO  / seconds - curCO)  < delta;
		isStableNO2 = abs(fltSumNO2 / seconds - curNO2) < delta;

		// Указатель на буфер
		pntrNH3 = (pntrNH3 + 1) % seconds;
		pntrCO  = (pntrCO  + 1) % seconds;
		pntrNO2 = (pntrNO2 + 1) % seconds;
	} while (!isStableNH3 || !isStableCO || !isStableNO2);

	_baseNH3 = fltSumNH3 / seconds;
	_baseCO  = fltSumCO  / seconds;
	_baseNO2 = fltSumNO2 / seconds;
}

void MICS6814::loadCalibrationData(
	uint16_t baseNH3,
	uint16_t baseCO,
	uint16_t baseNO2)
{
	_baseNH3 = baseNH3;
	_baseCO  = baseCO;
	_baseNO2 = baseNO2;
}

/**
 * Измеряет концентрацию газа в промилле для указанного газа.
 *
 * @param gas
 * Газ для расчета концентрации.
 *
 * @return
 * Текущая концентрация газа в частях на миллион (ppm).
 */
float MICS6814::measure(gas_t gas)
{
	float ratio;
	float c = 0;

	switch (gas)
	{
	case CO:
		ratio = getCurrentRatio(CH_CO);
		c = pow(ratio, -1.179) * 4.385;
		break;
	case NO2:
		ratio = getCurrentRatio(CH_NO2);
		c = pow(ratio, 1.007) / 6.855;
		break;
	case NH3:
		ratio = getCurrentRatio(CH_NH3);
		c = pow(ratio, -1.67) / 1.47;
		break;
	}

	return isnan(c) ? -1 : c;
}

/**
 * Запрашивает текущее сопротивление для данного канала от датчика. 
 * Значение - это значение АЦП в диапазоне от 0 до 1024.
 * 
 * @param channel
 * Канал для считывания базового сопротивления.
 *
 * @return
 * Беззнаковое 16-битное базовое сопротивление выбранного канала.
 */
uint16_t MICS6814::getResistance(channel_t channel) const
{
	unsigned long rs = 0;
	int counter = 0;

	switch (channel)
	{
	case CH_CO:
		for (int i = 0; i < 100; i++)
		{
			rs += analogRead(_pinCO);
			counter++;
			delay(2);
		}
	case CH_NO2:
		for (int i = 0; i < 100; i++)
		{
			rs += analogRead(_pinNO2);
			counter++;
			delay(2);
		}
	case CH_NH3:
		for (int i = 0; i < 100; i++)
		{
			rs += analogRead(_pinNH3);
			counter++;
			delay(2);
		}
	}

	return counter != 0 ? rs / counter : 0;
}

uint16_t MICS6814::getBaseResistance(channel_t channel) const
{
	switch (channel)
	{
	case CH_NH3:
		return _baseNH3;
	case CH_CO:
		return _baseCO;
	case CH_NO2:
		return _baseNO2;
	}

	return 0;
}

/**
 * Вычисляет коэффициент текущего сопротивления для данного канала.
 * 
 * @param channel
 * Канал для запроса значений сопротивления.
 *
 * @return
 * Коэффициент сопротивления с плавающей запятой для данного датчика.
 */
float MICS6814::getCurrentRatio(channel_t channel) const
{
	float baseResistance = (float)getBaseResistance(channel);
	float resistance = (float)getResistance(channel);

	return resistance / baseResistance * (1023.0 - baseResistance) / (1023.0 - resistance);

	return -1.0;
}