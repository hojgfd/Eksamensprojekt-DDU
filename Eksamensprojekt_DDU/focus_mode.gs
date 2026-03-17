function doGet(e) {

  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

  var timestamp = new Date();

  var duration = e.parameter.duration || "";
  var distractions = e.parameter.distractions || "";
  var score = e.parameter.score || "";

  sheet.appendRow([
    timestamp,
    duration,
    distractions,
    score,
    "M5Stack_1"
  ]);

  return ContentService.createTextOutput("OK");
}
