#include "adc.h"
#include "config.h"
#if 0
#include "ina236.h"
#endif

void ADCDriver::init()
{
    // GPIO2 = ADC1_CH1，ADC_ATTEN_DB_11 (Vmax=3100mV)
    analogReadResolution(ADC_BITS);
    analogSetPinAttenuation(ADC_PIN, ADC_11db);
}

uint32_t ADCDriver::read_raw()
{
    return analogRead(ADC_PIN);
}

int ADCDriver::read_mv()
{
    int mv = analogReadMilliVolts(ADC_PIN) + ADC_VOLTAGE_OFFSET_MV;
    return mv < 0 ? 0 : mv;
}

float ADCDriver::read_voltage_from_raw(uint32_t raw)
{
    // ESP-IDF: Vout = Dout * Vmax / Dmax
    return raw * (ADC_VMAX_MV / 1000.0f) / ADC_FULL_SCALE;
}

float ADCDriver::read_pin_voltage_v()
{
    return read_mv() / 1000.0f;
}

float ADCDriver::read_voltage_design_v(uint32_t raw)
{
    // 原理图设计换算: V = Dout / 4095 × 2.048V
    return (raw / ADC_FULL_SCALE) * ADC_VREF_V;
}

float ADCDriver::read_current_a()
{
    // V_adc = I × R × Gain + Vos × Gain  →  I = (V_adc/Gain - Vos) / R
    int mv_raw = analogReadMilliVolts(ADC_PIN);
    if (mv_raw < 0)
    {
        mv_raw = 0;
    }
    // 无负载时 ADC 接近 0，不做 mV/失调补偿，避免假电流
    if (mv_raw <= ADC_VOLTAGE_OFFSET_MV)
    {
        return 0.0f;
    }

    float v_adc = (mv_raw + ADC_VOLTAGE_OFFSET_MV) / 1000.0f;
    float v_shunt = v_adc / CURRENT_AMP_GAIN - CURRENT_AMP_INPUT_OFFSET_V;
    if (v_shunt < 0.0f)
    {
        v_shunt = 0.0f;
    }
    return v_shunt / CURRENT_SHUNT_OHM;
}

void ADCDriver::read_and_print()
{
    if (!hsadc_data) {
        return;
    }

    // uint32_t raw = read_raw();
    // int mv_cal = read_mv();
    // int mv_raw = (int)(read_voltage_from_raw(raw) * 1000.0f);
    // Serial.printf("AT+LS_ADC_RAW=%lu\r\n", raw);
    // Serial.printf("AT+LS_CURRENT_DATA_MV=%d\r\n", mv_cal);
    // Serial.printf("AT+LS_CURRENT_DATA_MV_RAW=%d\r\n", mv_raw);
    // Serial.printf("AT+LS_CURRENT_DATA_MV=%d\r\n", mv_cal);
    Serial.printf("AT+HS_CURRENT_DATA=%.3f\r\n", read_current_a());
#if 0
    INA236Driver::print_status();
#endif
    delay(100);
}
