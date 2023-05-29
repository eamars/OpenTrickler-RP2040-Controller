#include <string.h>
#include <stdlib.h>
#include "rest_endpoints.h"
#include "http_rest.h"
#include "charge_mode.h"
#include "motors.h"
#include "scale.h"
#include "wireless.h"
#include "eeprom.h"
#include "rotary_button.h"

const char index_page[] = "<!DOCTYPE html>\n"
                    "<html>\n"
                    "<head>\n"
                    "  <title>Scale Monitoring</title>\n"
                    "  <script src=\"https://cdn.plot.ly/plotly-latest.min.js\"></script>\n"
                    "  <script src=\"https://code.jquery.com/jquery-3.6.0.min.js\"></script>\n"
                    "</head>\n"
                    "<body>\n"
                    "  <h1>Scale Monitoring</h1>\n"
                    "  <h2>Current Weight: <span id=\"currentWeight\">Loading...</span></h2>\n"
                    "  <div id=\"chart\"></div>\n"
                    "\n"
                    "  <script>\n"
                    "    // Initialize the plot data\n"
                    "    var plotData = {\n"
                    "      x: [],\n"
                    "      y: [],\n"
                    "      mode: 'lines',\n"
                    "      type: 'scatter'\n"
                    "    };\n"
                    "\n"
                    "    // Initialize the plot layout\n"
                    "    var layout = {\n"
                    "      title: 'Weight over Time',\n"
                    "      xaxis: { title: 'Time' },\n"
                    "      yaxis: { title: 'Weight' }\n"
                    "    };\n"
                    "\n"
                    "    // Create an empty plot\n"
                    "    Plotly.newPlot('chart', [plotData], layout);\n"
                    "\n"
                    "    // Function to fetch the scale weight from the endpoint\n"
                    "    function fetchScaleWeight() {\n"
                    "      $.ajax({\n"
                    "        url: '/rest/scale_weight',\n"
                    "        type: 'GET',\n"
                    "        dataType: 'json',\n"
                    "        success: function (data) {\n"
                    "          // Update the current weight\n"
                    "          document.getElementById('currentWeight').textContent = data.weight.toFixed(3);\n"
                    "\n"
                    "          // Add the weight to the plot data\n"
                    "          var timestamp = new Date().getTime();\n"
                    "          plotData.x.push(timestamp);\n"
                    "          plotData.y.push(data.weight.toFixed(3));\n"
                    "\n"
                    "          // Prune old data\n"
                    "          var cutoffTime = timestamp - 20000;\n"
                    "          while (plotData.x[0] < cutoffTime) {\n"
                    "            plotData.x.shift();\n"
                    "            plotData.y.shift();\n"
                    "          }\n"
                    "\n"
                    "          // Update the plot\n"
                    "          Plotly.update('chart', [plotData], layout);\n"
                    "        },\n"
                    "        error: function (xhr, status, error) {\n"
                    "          console.error('Error fetching scale weight:', error);\n"
                    "        },\n"
                    "        complete: function () {\n"
                    "          // Schedule the next request after 1 second\n"
                    "          setTimeout(fetchScaleWeight, 250);\n"
                    "        }\n"
                    "      });\n"
                    "    }\n"
                    "\n"
                    "    // Start fetching the scale weight\n"
                    "    fetchScaleWeight();\n"
                    "  </script>\n"
                    "</body>\n"
                    "</html>";



bool http_404_error(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "{\"error\":404}";
    file->len = 13;
    file->index = 13;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_rest_index(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(index_page);

    file->data = index_page;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool rest_endpoints_init() {
    rest_register_handler("/", http_rest_index);
    rest_register_handler("/404", http_404_error);
    rest_register_handler("/rest/scale_weight", http_rest_scale_weight);
    rest_register_handler("/rest/scale_config", http_rest_scale_config);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/eeprom_config", http_rest_eeprom_config);
    rest_register_handler("/rest/motor_config", http_rest_motor_config);
    rest_register_handler("/rest/motor_speed", http_rest_motor_speed);
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
}