import glob
import time
import board
import busio
import adafruit_ads1x15.ads1115 as ADS
from adafruit_ads1x15.analog_in import AnalogIn
import psycopg2

base_dir = '/sys/bus/w1/devices/'
device_folder = glob.glob(base_dir + '28*')[0]
device_file = device_folder + '/w1_slave'

conn = psycopg2.connect(database="postgres", user = "jd@jdrs", password = "Flotuss.1", host = "jdrs.postgres.database.azure.com", port = "5432")
cur = conn.cursor()

# Ph Calibration values
calibrated_value = 19.34

# DO Calibration values
CAL1_V = 644
CAL1_T = 17.75

DO_Table = [14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410]

i2c = busio.I2C(board.SCL, board.SDA)
ads = ADS.ADS1115(i2c)
chan0 = AnalogIn(ads, ADS.P0)
chan1 = AnalogIn(ads, ADS.P1)

def read_do(voltage, temperature_c):
    V_saturation = CAL1_V + 35 * temperature_c - CAL1_T * 35
    return int(voltage * 1000 * DO_Table[int(temperature_c)] / V_saturation)

def read_temp_raw():
    f = open(device_file, 'r')
    lines = f.readlines()
    f.close()
    return lines

def read_temp():
    lines = read_temp_raw()
    while lines[0].strip()[-3:] != 'YES':
        time.sleep(0.2)
        lines = read_temp_raw()
    equals_pos = lines[1].find('t=')
    if equals_pos != -1:
        temp_string = lines[1][equals_pos+2:]
        temp_c = float(temp_string) / 1000.0
        # temp_f = temp_c * 9.0 / 5.0 + 32.0
        return temp_c

def read_ph(voltage):
    return -5.7 * voltage + calibrated_value

while True:
    temp_c, do, ph = 0, 0, 0
    for i in range(5):
        cur_temp = read_temp()
        temp_c += cur_temp
        do += read_do(chan0.voltage, cur_temp)
        ph += read_ph(chan1.voltage)
    temp_c /= 5
    do /= 5
    ph /= 5
    print(temp_c, do, ph)
    cur.execute("INSERT INTO ema_sensors(temperature, oxygen, ph) VALUES(%s, %s, %s)", (temp_c, do, ph))
    conn.commit()
    time.sleep(600)
conn.close()
