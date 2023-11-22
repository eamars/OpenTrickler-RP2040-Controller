"""
This script is created load the config.html as the Python REST frontend. The script will supply the JSON as the response to certain endpoints.

Usage

    python config_html_rest_test.py

Dependencies from pip:
 - flask
"""

import json
from flask import Flask
import os


script_directory = os.path.dirname(os.path.realpath(__file__))
config_html_path = os.path.join(script_directory, '..', 'src', 'html', 'config.html')
dashboard_html_path = os.path.join(script_directory, '..', 'src', 'html', 'dashboard.html')
static_folder = os.path.join(script_directory, '..', 'src', 'html')

app = Flask(__name__,
            static_url_path='',
            static_folder=static_folder)


@app.route('/rest/scale_config')
def rest_scale_config():
    return """{"driver":"AND FX-i Std","baudrate":19200}"""


@app.route('/rest/charge_mode_config')
def rest_charge_mode_config():
    return """{"c_kp":0.020,"c_ki":0.000,"c_kd":0.200,"f_kp":1.000,"f_ki":0.000,"f_kd":5.000,"c_stop":5.000,"f_stop":0.030,"sp_sd":0.020,"sp_avg":0.020}"""


@app.route('/rest/wireless_config')
def rest_wireless_config():
    return """{"ssid":"YYYY","pw":"xxx","auth":"CYW43_AUTH_WPA2_AES_PSK","timeout_ms":30000,"configured":true}"""

@app.route('/rest/eeprom_config')
def rest_eeprom_config():
    return """{"unique_id":"4C64A49","save_to_eeprom":false}'"""


@app.route('/rest/fine_motor_config')
def rest_fine_motor_config():
    return """{"m0":50.000000,"m1":200,"m2":800,"m3":256,"m4":5,"m5":110,"m6":0.050,"m7":2.105263,"m8":false,"m9":false}"""


@app.route('/rest/coarse_motor_config')
def rest_coarse_motor_config():
    return """{"m0":50.000000,"m1":200,"m2":800,"m3":256,"m4":5,"m5":110,"m6":0.050,"m7":1.250000,"m8":false,"m9":false}"""


@app.route('/rest/system_control')
def rest_system_control():
    return """{"unique_id":"8381FFF","save_to_eeprom":false,"software_reset":false,"erase_eeprom":false,"ver":"0.1.36-dirty","hash":"18111a5","build_type":"Debug"}"""


@app.route('/rest/neopixel_led_config')
def rest_neopixel_led_config():
    return """{"bl":"#ffffff","l1":"#0f0f0f","l2":"#0f0f0f"}"""


@app.route('/rest/button_config')
def rest_button_config():
    return """{"inv_dir":true}"""

@app.route('/rest/profile_config')
def rest_profile_config():
    return """{"pf":0,"p0":0,"p1":0,"p2":"AR2208,gr","p3":0.025,"p4":0.000,"p5":0.250,"p6":0.100,"p7":3.000,"p8":2.000,"p9":0.000,"p10":10.000,"p11":0.100,"p12":5.000}"""


@app.route("/config")
def config():
    with open(config_html_path) as fp:
        config_page = fp.read()
    return config_page


@app.route('/')
def dashboard():
    with open(dashboard_html_path) as fp:
        dashboard_page = fp.read()
    return dashboard_page


app.run(debug=True)
