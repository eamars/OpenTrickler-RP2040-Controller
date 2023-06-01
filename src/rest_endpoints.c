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


const char html_display_mirror[] = "<!doctype html><title>Pixel Renderer</title><style>canvas{border:1px solid black}</style><body><canvas id=pixel-canvas></canvas><script>const canvas=document.getElementById(\"pixel-canvas\");const context=canvas.getContext(\"2d\");function renderPixel(a,b,c){context.fillStyle=c;context.fillRect(a,b,4,4)}const scaleFactor=4;canvas.width=128*scaleFactor;canvas.height=64*scaleFactor;function fetchAndRender(){fetch(\"/display_buffer\").then(a=>a.arrayBuffer()).then(a=>{const b=new Uint8Array(a);const c=0x10;for(let d=0;d<8;d++){for(let e=0;e<8;e++){for(let f=0;f<c*8;f++){const g=f+ d*c*8;let h;try{h=b[g]}catch(j){console.log(g);throw j};const i=1<<e&h?\"black\":\"white\";renderPixel(f*scaleFactor,d*8*scaleFactor+ e*scaleFactor,i)}}}}).catch(a=>{console.log(\"Error fetching binary data:\",a)})}setInterval(fetchAndRender,1000)</script>";

const char html_plot_weight[] = "<!doctype html><title>Scale Monitoring</title><script src=https://cdn.plot.ly/plotly-latest.min.js></script><script src=https://code.jquery.com/jquery-3.6.0.min.js></script><body><h1>Scale Monitoring</h1><h2>Current Weight: <span id=currentWeight>Loading...</span></h2><div id=chart></div><script>var plotData={x:[],y:[],mode:\'lines\',type:\'scatter\'};var layout={title:\'Weight over Time\',xaxis:{title:\'Time\'},yaxis:{title:\'Weight\'}};Plotly.newPlot(\'chart\',[plotData],layout);function fetchScaleWeight(){$.ajax({url:\'/rest/scale_weight\',type:\'GET\',dataType:\'json\',success:function(a){document.getElementById(\'currentWeight\').textContent=a.weight.toFixed(3);var b=new Date().getTime();plotData.x.push(b);plotData.y.push(a.weight.toFixed(3));var c=b- 20000;while(plotData.x[0]<c){plotData.x.shift();plotData.y.shift()};Plotly.update(\'chart\',[plotData],layout)},error:function(a,b,c){console.error(\'Error fetching scale weight:\',c)},complete:function(){setTimeout(fetchScaleWeight,500)}})}fetchScaleWeight()</script>";



bool http_404_error(struct fs_file *file, int num_params, char *params[], char *values[]) {

    file->data = "{\"error\":404}";
    file->len = 13;
    file->index = 13;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;

    return true;
}


bool http_plot_weight(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_plot_weight);

    file->data = html_plot_weight;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}

bool http_display_mirror(struct fs_file *file, int num_params, char *params[], char *values[]) {
    size_t len = strlen(html_display_mirror);

    file->data = html_display_mirror;
    file->len = len;
    file->index = len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_HEADER_PERSISTENT;

    return true;
}


bool rest_endpoints_init() {
    // rest_register_handler("/", http_rest_index);
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
    rest_register_handler("/display_mirror", http_display_mirror);
    rest_register_handler("/plot_weight", http_plot_weight);
}