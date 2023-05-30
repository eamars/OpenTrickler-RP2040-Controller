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
#include "display.h"


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


const char render_display[] = "\
<!DOCTYPE html>\n\
<html>\n\
  <head>\n\
    <title>Pixel Renderer</title>\n\
    <style>\n\
      canvas {\n\
        border: 1px solid black;\n\
      }\n\
    </style>\n\
  </head>\n\
  <body>\n\
    <canvas id=\"pixel-canvas\"></canvas>\n\
\n\
    <script>\n\
      const canvas = document.getElementById(\"pixel-canvas\");\n\
      const context = canvas.getContext(\"2d\");\n\
\n\
      function renderPixel(x, y, color) {\n\
        context.fillStyle = color;\n\
        context.fillRect(x, y, 4, 4);\n\
      }\n\
\n\
      const scaleFactor = 4;\n\
      canvas.width = 128 * scaleFactor;\n\
      canvas.height = 64 * scaleFactor;\n\
\n\
      function fetchAndRender() {\n\
        fetch(\"/display_buffer\")\n\
          .then((response) => response.arrayBuffer())\n\
          .then((buffer) => {\n\
            const binaryData = new Uint8Array(buffer);\n\
\n\
            const tileWidth = 0x10;\n\
\n\
            for (let tileRowIdx = 0; tileRowIdx < 8; tileRowIdx++) {\n\
              for (let bit = 0; bit < 8; bit++) {\n\
                for (let byteIdx = 0; byteIdx < tileWidth * 8; byteIdx++) {\n\
                  const dataOffset = byteIdx + tileRowIdx * tileWidth * 8;\n\
                  let data;\n\
\n\
                  try {\n\
                    data = binaryData[dataOffset];\n\
                  } catch (error) {\n\
                    throw error;\n\
                  }\n\
\n\
                  const color = (1 << bit) & data ? \"black\" : \"white\";\n\
                  renderPixel(byteIdx * scaleFactor, tileRowIdx * 8 * scaleFactor + bit * scaleFactor, color);\n\
                }\n\
              }\n\
            }\n\
          })\n\
          .catch((error) => {\n\
            throw error;\n\
          });\n\
      }\n\
\n\
      setInterval(fetchAndRender, 1000);\n\
    </script>\n\
  </body>\n\
</html>";


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
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}

bool http_render_display(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(render_display);

    file->data = render_display;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool rest_endpoints_init() {
    rest_register_handler("/", http_rest_index);
    rest_register_handler("/404", http_404_error);
    rest_register_handler("/rest/scale_weight", http_rest_scale_weight);
    rest_register_handler("/rest/scale_config", http_rest_scale_config);
    rest_register_handler("/rest/charge_mode_config", http_rest_charge_mode_config);
    rest_register_handler("/rest/charge_mode_setpoint", http_rest_charge_mode_setpoint);
    rest_register_handler("/rest/eeprom_config", http_rest_eeprom_config);
    rest_register_handler("/rest/motor_config", http_rest_motor_config);
    rest_register_handler("/rest/motor_speed", http_rest_motor_speed);
    rest_register_handler("/rest/button_control", http_rest_button_control);
    rest_register_handler("/rest/wireless_config", http_rest_wireless_config);
    rest_register_handler("/display_buffer", http_get_display_buffer);
    rest_register_handler("/render_display", http_render_display);
}