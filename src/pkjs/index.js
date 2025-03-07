var Clay = require("pebble-clay");
var clayConfig = require("./config");
var clay = new Clay(clayConfig);

function weatherIdToIconIndex(weatherId) {
  var weatherCodes = {
    clear: [0, 1],
    clouds: [2, 3],
    fog: [45, 48],
    rain: [51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82],
    snow: [71, 73, 75, 77, 85, 86],
    storm: [95, 96, 99],
  };
  for (var key in weatherCodes) {
    if (weatherCodes[key].indexOf(weatherId) !== -1) {
      return Object.keys(weatherCodes).indexOf(key);
    }
  }
}

function sendMessage(message) {
  Pebble.sendAppMessage(
    message,
    function (e) {
      console.log("Weather info sent to Pebble successfully!");
    },
    function (e) {
      console.log("Error sending weather info to Pebble!");
    }
  );
}

function getLocation(lat, lon, api_key, message) {
  var location_request = new XMLHttpRequest();
  location_request.onload = function () {
    var location_response = JSON.parse(location_request.responseText);
    message.LOCATION = location_response[0].name;
    sendMessage(message);
  };
  var owm_url =
    "http://api.openweathermap.org/geo/1.0/reverse?lat=" +
    lat +
    "&lon=" +
    lon +
    "&appid=" +
    api_key;
  location_request.open("GET", owm_url);
  location_request.send();
}

function getWeatherData() {
  var settings = JSON.parse(localStorage.getItem("clay-settings")) || {};
  var meteo_units = settings.UNITS === "F" ? "fahrenheit" : "celsius";
  var owm_key = settings.OWM_API_KEY;
  var location_name = settings.LOCATION_NAME || "My Location";
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      var weather_request = new XMLHttpRequest();
      weather_request.onload = function () {
        var weather_response = JSON.parse(weather_request.responseText);

        var message = {
          CUR_TEMP: Math.round(weather_response.current.temperature_2m),
          HIGH_TEMP: Math.round(weather_response.daily.temperature_2m_max[0]),
          LOW_TEMP: Math.round(weather_response.daily.temperature_2m_min[0]),
          CONDITIONS: weatherIdToIconIndex(
            weather_response.current.weather_code
          ),
          LOCATION: location_name,
        };
        if (owm_key) {
          getLocation(
            pos.coords.latitude,
            pos.coords.longitude,
            owm_key,
            message
          );
        } else {
          sendMessage(message);
        }
      };
      var daily_url =
        "https://api.open-meteo.com/v1/forecast?latitude=" +
        pos.coords.latitude +
        "&longitude=" +
        pos.coords.longitude +
        "&current=temperature_2m,weather_code" +
        "&daily=temperature_2m_max,temperature_2m_min" +
        "&temperature_unit=" +
        meteo_units +
        "&timezone=auto&forecast_days=1";
      weather_request.open("GET", daily_url);
      weather_request.send();
    },
    function (err) {
      console.log("Error requesting location!");
    },
    {
      timeout: 15000,
      maximumAge: 60000,
    }
  );
}

Pebble.addEventListener("ready", function () {
  getWeatherData();
});

Pebble.addEventListener("appmessage", function (e) {
  getWeatherData();
});
