#include <WiFi.h>
#include <WebServer.h>
#include <sps30.h>

/*This code implements a lightweight web server using an ESP32 that hosts a real-time dashboard for particulate matter sensor data.
The server serves an HTML page with a Chart.js-based graph, which fetches updated sensor data every 2 seconds using AJAX. 
The ESP32 reads the SPS30 sensor on demand when data is requested by the client. The routing and request handling are straightforward, 
with two key routes: one for the main webpage and one for the JSON data.*/
const char* ssid = "";
const char* password = "";

WebServer server(80);

struct sps30_measurement m;
char serial[SPS30_MAX_SERIAL_LEN];

// Function Prototypes
void handleRoot();
void handleData();

// HTML content served by the ESP32
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Real-Time Sensor Data</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-streaming@2.0.0"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@3.0.0"></script> 
</head>
<body>
  <h2>ESP32 Real-Time SPS30 Sensor Data</h2>
  <canvas id="myChart" width="200" height="100"></canvas>
  <script>
  Chart.register(ChartStreaming);
    var chartData = {
      datasets: [
        {
          label: 'PM1.0 (µg/m³)',
          backgroundColor: 'rgba(255, 99, 132, 0.2)',
          borderColor: 'rgba(255, 99, 132, 1)',
          data: []
        },
        {
          label: 'PM2.5 (µg/m³)',
          backgroundColor: 'rgba(54, 162, 235, 0.2)',
          borderColor: 'rgba(54, 162, 235, 1)',
          data: []
        },
        {
          label: 'PM4.0 (µg/m³)',
          backgroundColor: 'rgba(75, 192, 192, 0.2)',
          borderColor: 'rgba(75, 192, 192, 1)',
          data: []
        },
        {
          label: 'PM10.0 (µg/m³)',
          backgroundColor: 'rgba(153, 102, 255, 0.2)',
          borderColor: 'rgba(153, 102, 255, 1)',
          data: []
        }
      ]
    };

    var config = {
      type: 'line',
      data: chartData,
      options: {
        scales: {
          x: {
            type: 'realtime', // Use the realtime scale provided by the plugin
            realtime: {
              delay: 2000,
              onRefresh: function(chart) {
                fetch('/data')
                  .then(response => response.json())
                  .then(data => {
                    var now = Date.now();
                    chart.data.datasets[0].data.push({ x: now, y: data.pm10 }); // PM 1.0
                    chart.data.datasets[1].data.push({ x: now, y: data.pm25 }); // PM 2.5
                    chart.data.datasets[2].data.push({ x: now, y: data.pm40 }); // PM 4.0
                    chart.data.datasets[3].data.push({ x: now, y: data.pm100 }); // PM 10.0

                    // Remove old data to keep the graph from getting too cluttered
                    chart.data.datasets.forEach(dataset => {
                      if (dataset.data.length > 50) dataset.data.shift(); // Keep last 50 data points
                    });

                    chart.update('quiet');
                  });
              }
            }
          }
        }
      }
    };
    var myChart = new Chart(
      document.getElementById('myChart'),
      config
    );
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(9600);
  delay(2000);

  sensirion_i2c_init();

  while (sps30_probe() != 0) {
    Serial.print("SPS sensor probing failed\n");
    delay(500);
  }

  Serial.print("SPS sensor probing successful\n");

  if (sps30_start_measurement() < 0) {
    Serial.print("error starting measurement\n");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up routes
  //The first route ("/") serves the main HTML page, while the second route ("/data") serves the sensor data.
  server.on("/", handleRoot);//handles what to display when the url is just 10.0.0.37
  server.on("/data", handleData);//handles what to display when the url is 10.0.0.37/data

  // Start the web server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  // Handle client requests depending on the url server route outlined above 
  server.handleClient();// Continuously checks for and handles incoming client requests.
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);//sends the browser the html page that was created
}

void handleData() {
  if (sps30_read_measurement(&m) >= 0) {
    String jsonData = "{";
    jsonData += "\"pm10\":" + String(m.mc_1p0) + ","; // PM 1.0
    jsonData += "\"pm25\":" + String(m.mc_2p5) + ","; // PM 2.5
    jsonData += "\"pm40\":" + String(m.mc_4p0) + ","; // PM 4.0
    jsonData += "\"pm100\":" + String(m.mc_10p0);     // PM 10.0
    jsonData += "}";
    server.send(200, "application/json", jsonData);
  } else {
    server.send(500, "application/json", "{\"error\":\"Failed to read measurement\"}");
  }
}
